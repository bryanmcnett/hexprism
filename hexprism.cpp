#include "stdio.h"
#include <vector>
#include <time.h>
#include <math.h>
#include <immintrin.h>
#include <stdint.h>

struct Clock
{
  const clock_t m_start;
  Clock() : m_start(clock())
  {
  }
  float seconds() const
  {
    const clock_t end = clock();
    const float seconds = ((float)(end - m_start)) / CLOCKS_PER_SEC;
    return seconds;
  }
};

#define VECTOR __m128
#define MASK __m128
#define SIZE 4

union UNION
{
  VECTOR v;
  float f[SIZE];
};

float get(VECTOR v, int index)
{
  UNION u;
  u.v = v;
  return u.f[index];
}

VECTOR broadcast(VECTOR v, float value)
{
  UNION u;
  u.v = v;
  for(int i = 0; i < SIZE; ++i)
    u.f[i] = value;
  return u.v;
}

VECTOR broadcast_index(VECTOR v, int index)
{
  return broadcast(v, get(v, index));
}

void set(VECTOR& v, int index, float value)
{
  UNION u;
  u.v = v;
  u.f[index] = value;
  v = u.v;
}

uint32_t cmple_ps(float a, float b) { return a <= b ? 0xFFFFFFFF : 0; }
uint32_t and_ps(uint32_t a, uint32_t b) { return a & b; }
uint32_t movemask_ps(uint32_t a) { return (a >> 31) & 1; }

__m128 cmple_ps(__m128 a, __m128 b) { return _mm_cmple_ps(a,b); }
__m128 and_ps(__m128 a, __m128 b) { return _mm_and_ps(a,b); }
uint32_t movemask_ps(__m128 a) { return _mm_movemask_ps(a); }

#if 0
__m256 cmple_ps(__m256 a, __m256 b) { return _mm256_cmp_ps(a,b,_CMP_LE_OQ); }
__m256 and_ps(__m256 a, __m256 b) { return _mm256_and_ps(a,b); }
uint32_t movemask_ps(__m256 a) { return _mm256_movemask_ps(a); }
#endif

struct Slab
{
  VECTOR mini, maxi;
};

struct TwoSlab
{
  Slab x, y;
};

struct AABB
{
  TwoSlab xy;
  Slab z;
};

struct AABBs
{
  TwoSlab *xy;
  Slab *z;
};

MASK Intersects(const TwoSlab a, const TwoSlab b)
{
  MASK mask =              cmple_ps(a.x.mini, b.x.maxi);
       mask = and_ps(mask, cmple_ps(b.x.mini, a.x.maxi));
       mask = and_ps(mask, cmple_ps(a.y.mini, b.y.maxi));
       mask = and_ps(mask, cmple_ps(b.y.mini, a.y.maxi));
  return mask;
}

int Intersects(const AABBs world, const int index, const AABB query)
{
  MASK mask = Intersects(world.xy[index], query.xy); // 4 half-spaces
  if(movemask_ps(mask) == 0)
    return 0;
  mask = and_ps(mask, cmple_ps(query.z.mini, world.z[index].maxi)); // 1 half-space
  mask = and_ps(mask, cmple_ps(world.z[index].mini, query.z.maxi)); // 1 half-space
  return movemask_ps(mask);
};

struct TriangleUp
{
  VECTOR minA, minB, minC;
};

struct TriangleDown
{
  VECTOR maxA, maxB, maxC;
};

MASK Intersects(const TriangleUp up, const TriangleDown down)
{
  MASK mask =              cmple_ps(up.minA, down.maxA);
       mask = and_ps(mask, cmple_ps(up.minB, down.maxB));
       mask = and_ps(mask, cmple_ps(up.minC, down.maxC));
  return mask;
}

struct HexPrism
{
  TriangleUp up;
  TriangleDown down;
  Slab z;
};

struct HexPrisms
{
  TriangleUp *up;
  TriangleDown *down;
  Slab *z;
};

int Intersects(const HexPrisms world, const int index, const HexPrism query)
{
  MASK mask = Intersects(world.up[index], query.down); // 3 half-spaces
  if(movemask_ps(mask) == 0)
    return 0;
  mask = and_ps(mask, Intersects(query.up, world.down[index])); // 3 half-spaces
  mask = and_ps(mask, cmple_ps(query.z.mini, world.z[index].maxi)); // 1 half-space
  mask = and_ps(mask, cmple_ps(world.z[index].mini, query.z.maxi)); // 1 half-space
  return movemask_ps(mask);
};

struct float3
{
  float x,y,z;
};
  
float3 operator+(const float3 a, const float3 b)
{
  float3 c = {a.x+b.x, a.y+b.y, a.z+b.z};
  return c;
}

float dot(const float3 a, const float3 b)
{
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

float length(const float3 a)
{
  return sqrtf(dot(a,a));
}

float3 min(const float3 a, const float3 b)
{
  float3 c = {std::min(a.x,b.x), std::min(a.y,b.y), std::min(a.z,b.z)};
  return c;
}

float3 max(const float3 a, const float3 b)
{
  float3 c = {std::max(a.x,b.x), std::max(a.y,b.y), std::max(a.z,b.z)};
  return c;
}

struct float4
{
  float a,b,c,d;
};

float4 min(const float4 a, const float4 b)
{
  float4 c = {std::min(a.a,b.a), std::min(a.b,b.b), std::min(a.c,b.c), std::min(a.d,b.d)};
  return c;
}

float4 max(const float4 a, const float4 b)
{
  float4 c = {std::max(a.a,b.a), std::max(a.b,b.b), std::max(a.c,b.c), std::max(a.d,b.d)};
  return c;
}

float random(float lo, float hi)
{
  const int grain = 10000;
  const float t = (rand() % grain) * 1.f/(grain-1);
  return lo + (hi - lo) * t;
}

struct Mesh
{
  std::vector<float3> m_point;
  void Generate(int points, float radius)
  {
    m_point.resize(points);
    for(int p = 0; p < points; ++p)
    {
      do
      {
        m_point[p].x = random(-radius, radius) * 0.25f;
        m_point[p].y = random(-radius, radius) * 0.25f;
        m_point[p].z = random(-radius, radius);  // mostly taller than wide, like in a game
      } while(length(m_point[p]) > radius);
    }
  }
};

struct Object
{
  Mesh *m_mesh;
  float3 m_position;
  void CalculateAABB(AABBs* aabb, int index) const
  {
    float3 mini, maxi;
    const float3 xyz = m_position + m_mesh->m_point[0];
    mini = maxi = xyz;
    for(int p = 1; p < m_mesh->m_point.size(); ++p)
    {
      const float3 xyz = m_position + m_mesh->m_point[p];
      mini = min(mini, xyz);
      maxi = max(maxi, xyz);
    }
    const int vector  = index / SIZE;
    const int element = index % SIZE;
    set(aabb->xy[vector].x.mini, element, mini.x);
    set(aabb->xy[vector].x.maxi, element, maxi.x);
    set(aabb->xy[vector].y.mini, element, mini.y);
    set(aabb->xy[vector].y.maxi, element, maxi.y);
    set(aabb->z[vector].mini, element, mini.z);
    set(aabb->z[vector].maxi, element, maxi.z);
  }
  void CalculateHexPrism(HexPrisms* hexPrism, int index) const
  { 
    const float3 xyz = m_position + m_mesh->m_point[0];
    float4 abcd, mini, maxi;
    abcd.a = xyz.x;
    abcd.b = xyz.y;
    abcd.c = -(xyz.x + xyz.y);
    abcd.d = xyz.z;
    mini = maxi = abcd;
    for(int p = 1; p < m_mesh->m_point.size(); ++p)
    {
      const float3 xyz = m_position + m_mesh->m_point[p];
      abcd.a = xyz.x;
      abcd.b = xyz.y;
      abcd.c = -(xyz.x + xyz.y);
      abcd.d = xyz.z;
      mini = min(mini, abcd);
      maxi = max(maxi, abcd);
    }
    const int vector  = index / SIZE;
    const int element = index % SIZE;
    set(hexPrism->up[vector].minA, element, mini.a);
    set(hexPrism->up[vector].minB, element, mini.b);
    set(hexPrism->up[vector].minC, element, mini.c);
    set(hexPrism->down[vector].maxA, element, maxi.a);   
    set(hexPrism->down[vector].maxB, element, maxi.b);
    set(hexPrism->down[vector].maxC, element, maxi.c);
    set(hexPrism->z[vector].mini, element, mini.d);
    set(hexPrism->z[vector].maxi, element, maxi.d);
  };
};

int main(int argc, char* argv[])
{
  const int kMeshes = 100;
  Mesh mesh[kMeshes];
  for(int m = 0; m < kMeshes; ++m)
    mesh[m].Generate(50, 1.f);

  const int kTests = 500;

  const int kVectors = 10000000;
  const int kObjects = kVectors / SIZE;
  Object* objects = new Object[kObjects];
  for(int o = 0; o < kObjects; ++o)
  {
    objects[o].m_mesh = &mesh[rand() % kMeshes];
    objects[o].m_position.x = random(-1000.f, 1000.f);
    objects[o].m_position.y = random(-1000.f, 1000.f);
    objects[o].m_position.z = random(   -1.f,    1.f); // mostly wider than flat, like in a game
  }

  AABBs aabbs;
  aabbs.xy = new TwoSlab[kVectors];
  aabbs.z  = new Slab[kVectors];
  for(int a = 0; a < kObjects; ++a)
    objects[a].CalculateAABB(&aabbs, a);

  HexPrisms hexprisms;
  hexprisms.up   = new TriangleUp[kVectors];
  hexprisms.down = new TriangleDown[kVectors];
  hexprisms.z    = new Slab[kVectors];
  for(int a = 0; a < kObjects; ++a)
    objects[a].CalculateHexPrism(&hexprisms, a);

  const char *title = "%22s | %7s | %7s\n";

  printf(title, "bounding volume", "accepts", "seconds");
  printf("------------------------------------------\n");
  
  const char *format = "%22s | %7d |  %3.4f\n";
  
  {
    const Clock clock;
    int intersections = 0;
    for(int test = 0; test < kTests; ++test)
    {
      AABB query;
      query.xy.x.mini = broadcast_index(aabbs.xy[test].x.mini, 0);
      query.xy.x.maxi = broadcast_index(aabbs.xy[test].x.maxi, 0);
      query.xy.y.mini = broadcast_index(aabbs.xy[test].y.mini, 0);
      query.xy.y.maxi = broadcast_index(aabbs.xy[test].y.maxi, 0);
      query.z.mini    = broadcast_index(aabbs.z[test].mini, 0);
      query.z.maxi    = broadcast_index(aabbs.z[test].maxi, 0);
      for(int v = 0; v < kVectors; ++v)
	if(const int mask = Intersects(aabbs, v, query))
	  intersections += __builtin_popcount(mask);
    }
    const float seconds = clock.seconds();
    
    printf(format, "AABB", intersections, seconds);
  }

  {
    const Clock clock;
    int intersections = 0;
    for(int test = 0; test < kTests; ++test)
    {
      HexPrism query;
      query.up.minA   = broadcast_index(hexprisms.up[test].minA, 0);
      query.up.minB   = broadcast_index(hexprisms.up[test].minB, 0);
      query.up.minC   = broadcast_index(hexprisms.up[test].minC, 0);
      query.down.maxA = broadcast_index(hexprisms.down[test].maxA, 0);
      query.down.maxB = broadcast_index(hexprisms.down[test].maxB, 0);
      query.down.maxC = broadcast_index(hexprisms.down[test].maxC, 0);
      query.z.mini    = broadcast_index(hexprisms.z[test].mini, 0);
      query.z.maxi    = broadcast_index(hexprisms.z[test].maxi, 0);
      for(int v = 0; v < kVectors; ++v)
	if(const int mask = Intersects(hexprisms, v, query))
	  intersections += __builtin_popcount(mask);
    }
    const float seconds = clock.seconds();
    
    printf(format, "HexPrism", intersections, seconds);
  }
  
  return 0;
}
