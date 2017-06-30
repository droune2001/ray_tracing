#ifndef _RAYTRACER_PERLIN_H_
#define _RAYTRACER_PERLIN_H_

#include "random_vectors.h"

internal const int X_NOISE_GEN = 1619;
internal const int Y_NOISE_GEN = 31337;
internal const int Z_NOISE_GEN = 6971;
internal const int SEED_NOISE_GEN = 1013;
internal const int SHIFT_NOISE_GEN = 8;

template<class T>
inline T scurve3(T a)
{
    return (a*a*((T)3.0-(T)2.0*a));
}

template<class T>
inline T scurve5(T a)
{
    T a3 = a*a*a;
    T a4 = a3*a;
    T a5 = a4*a;
    return ((T)6.0*a5) - ((T)15.0*a4) + ((T)10.0*a3);
}

template<class T>
inline T linear_interp( T n0, T n1, T a )
{
    return (((T)1.0 - a) * n0) + (a * n1);
}

inline float trilinear_interp( float c[2][2][2], float u, float v, float w )
{
    float accum = 0.0f;
    for ( int i = 0; i < 2; ++i )
    {
        for ( int j = 0; j < 2; ++j )
        {
            for ( int k = 0; k < 2; ++k )
            {
                accum += 
                    (i*u + (1-i)*(1-u)) *
                    (j*v + (1-j)*(1-v)) *
                    (k*w + (1-k)*(1-w)) *
                    c[i][j][k];
            }
        }
    }
    
    return accum;
}

inline float perlin_interp( vec3 c[2][2][2], float u, float v, float w )
{
    float uu = scurve3(u);
    float vv = scurve3(v);
    float ww = scurve3(w);
    float accum = 0.0f;
    for ( int i = 0; i < 2; ++i )
    {
        for ( int j = 0; j < 2; ++j )
        {
            for ( int k = 0; k < 2; ++k )
            {
                vec3 weight_v( u-i, v-j, w-k );
                accum += 
                    (i*uu + (1.0f-i)*(1.0f-uu)) *
                    (j*vv + (1.0f-j)*(1.0f-vv)) *
                    (k*ww + (1.0f-k)*(1.0f-ww)) *
                    dot( c[i][j][k], weight_v );
            }
        }
    }
    
    return accum;
}

struct perlin
{
    float noise( const vec3 &p ) const 
    {
        float u = p.x() - floorf(p.x());
        float v = p.y() - floorf(p.y());
        float w = p.z() - floorf(p.z());
        
        int i = int(floorf(p.x()));
        int j = int(floorf(p.y()));
        int k = int(floorf(p.z()));
        
        vec3 c[2][2][2];
        for ( int di = 0; di < 2; ++di )
        {
            for ( int dj = 0; dj < 2; ++dj )
            {
                for ( int dk = 0; dk < 2; ++dk )
                {
                    const int seed = 1;
                    int index = (
                        X_NOISE_GEN * (i+di)
                        + Y_NOISE_GEN * (j+dj)
                        + Z_NOISE_GEN * (k+dk)
                        + SEED_NOISE_GEN * seed ) & 0xffffffff;
                    
                    index ^= (index >> SHIFT_NOISE_GEN);
                    index &= 0xff;
                    
                    /*int index = 
                        perm_x[(i+di)&255]^
                        perm_y[(j+dj)&255]^
                        perm_z[(k+dk)&255];
                    */
                    
                    c[di][dj][dk] = vec3((float)g_randomVectors[(index << 2)], (float)g_randomVectors[(index << 2) + 1],
                                         (float)g_randomVectors[(index << 2) + 2]);
                }
            }
        }
        //        return trilinear_interp(c,u,v,w);
        return perlin_interp(c,u,v,w);
    }
    
    float turb( const vec3 &p, int depth = 7 ) const
    {
        float accum = 0.0f;
        vec3 temp_p = p;
        float weight = 1.0f;
        for ( int i = 0; i < depth; ++i )
        {
            accum += weight * noise( temp_p );
            weight *= 0.5f;
            temp_p *= 2.0f;
        }
        
        return fabsf( accum );
    }
    
    static vec3 *ranvec;
    static int *perm_x;
    static int *perm_y;
    static int *perm_z;
};

internal
vec3 *perlin_generate()
{
    vec3 *p = new vec3[256];
    for ( int i = 0; i < 256; ++i )
    {
        // random [-1;1] vectors
        p[i] = unit_vector(vec3(-1+2*RAN01(),-1+2*RAN01(),-1+2*RAN01()));
    }
    
    return p;
}

void permute( int *p, int n )
{
    for ( int i = n-1; i >= 0; --i )
    {
        int target = int(RAN01()*(i+1));
        int tmp = p[i];
        p[i] = p[target];
        p[target] = tmp;
    }
    
    return;
}

internal
int *perlin_generate_perm()
{
    int *p = new int[256];
    for ( int i = 0; i < 256; ++i )
    {
        p[i] = i;
    }
    permute( p, 256 );
    return p;
}

vec3 *perlin::ranvec = perlin_generate();
int *perlin::perm_x = perlin_generate_perm();
int *perlin::perm_y = perlin_generate_perm();
int *perlin::perm_z = perlin_generate_perm();

#endif //_RAYTRACER_PERLIN_H_
