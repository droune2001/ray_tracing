#ifndef _RAYTRACER_TEXTURE_H_
#define _RAYTRACER_TEXTURE_H_

struct texture
{
    virtual vec3 value( float u, float v, const vec3 &p ) const = 0;
};

struct constant_texture : public texture
{
    constant_texture() {}
    constant_texture( const vec3 &c) : color(c) {}
    virtual vec3 value( float u, float v, const vec3 &p ) const override
    {
        return color;
    }
    
    vec3 color;
};

struct checker_texture : public texture
{
    checker_texture() {}
    checker_texture(texture *t0, texture *t1) : odd(t0), even(t1) {}
    
    virtual vec3 value( float u, float v, const vec3 &p ) const override
    {
        float sines = sin(10.0f*p.x()) * sin(10.0f*p.y()) * sin(10.0f*p.z());
        if ( sines < 0.0f )
        {
            return odd->value(u,v,p);
        }
        else
        {
            return even->value(u,v,p);
        }
    }
    
    texture *odd;
    texture *even;
};

struct noise_texture : public texture
{
    noise_texture() {}
    noise_texture( float sc ) : scale( sc ) {}
    virtual vec3 value( float u, float v, const vec3 &p ) const override
    {
        //return vec3( 1.0f, 1.0f, 1.0f ) * noise.noise( scale * p );
        //return vec3( 1.0f, 1.0f, 1.0f ) * noise.turb( scale * p, 7 );
        return vec3( 1.0f, 1.0f, 1.0f ) * 0.5f * 
            (1.0f + sinf(scale * p.z() + 10.0f * noise.turb( scale * p, 7 )));
    }
    
    perlin noise;
    float scale;
};

struct image_texture : public texture
{
    image_texture() {}
    image_texture( unsigned char *pixels, int A, int B ) : data(pixels), nx(A), ny(B) {}
    virtual vec3 value( float u, float v, const vec3 &p ) const override
    {
        int i = int(u*(float)nx); // [0..1] -> [0..w]
        int j = int((1.0f-v)*(float)ny-0.001f); // [0..1] -> [0..h] flipped
        // clamp (could wrap...)
        if ( i < 0 ) i = 0;
        if ( j < 0 ) j = 0;
        if ( i > nx - 1 ) i = nx - 1;
        if ( j > ny - 1 ) j = ny - 1;
        float r = float(data[3*i + 3*nx*j    ]) / 255.0f;
        float g = float(data[3*i + 3*nx*j + 1]) / 255.0f;
        float b = float(data[3*i + 3*nx*j + 2]) / 255.0f;
        
        return vec3( r, g, b );
    }
    
    unsigned char *data;
    int nx, ny;
};

#endif // _RAYTRACER_TEXTURE_H_
