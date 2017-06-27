#ifndef _RAYTRACER_TEXTURE_H_
#define _RAYTRACER_TEXTURE_H_

struct texture
{
    virtual vec3 value( float u, float v, const vec3 &p ) = 0;
};

struct constant_texture : public texture
{
    constant_texture() {}
    constant_texture( const vec3 &c) : color(c) {}
    virtual vec3 value( float u, float v, const vec3 &p ) override
    {
        return color;
    }
    
    vec3 color;
};

struct checker_texture : public texture
{
    checker_texture() {}
    checker_texture(texture *t0, texture *t1) : odd(t0), even(t1) {}
    
    virtual vec3 value( float u, float v, const vec3 &p ) override
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

#endif // _RAYTRACER_TEXTURE_H_
