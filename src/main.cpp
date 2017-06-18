#include <iostream>
#include <cmath>
#include <cstdlib>

#include "vec3.h"
#include "ray.h"

bool hit_sphere( const vec3 &center, float radius, const ray &r )
{
    vec3 oc = r.origin() - center;
    float a = dot( r.direction(), r.direction() );
    float b = 2.0f * dot( oc, r.direction() );
    float c = dot( oc, oc ) - radius * radius;
    float discriminant = b*b - 4*a*c;
    return (discriminant > 0.0f);
}

vec3 color( const ray &r)
{
    // sphere at z = -1 -> in red
    if (hit_sphere(vec3(0,0,-1), 0.5f, r))
    {
        return vec3(1.0f, 0.0f, 0.0f);
    }
    
    // BACKGROUND COLOR
    vec3 unit_direction = unit_vector(r.direction());
    // Y from [-1;1] to [0;1]
    float t = 0.5f * (unit_direction.y() + 1.0f);
    // gradient from blue to white
    return (1.0f - t) * vec3(1.0f, 1.0f, 1.0f) + t * vec3(0.5f, 0.7f, 1.0f);
}

int main( int argc, char **argv )
{
    int nx = 200;
    int ny = 100;
    std::cout << "P3\n" << nx << " " << ny << "\n255\n";
    
    // screen is a 4/2 ratio rect at z = -1. Camera is at 0,0,0.
    vec3 lower_left_corner(-2.0f, -1.0f, -1.0f);
    vec3 horizontal(4.0f, 0.0f, 0.0f);
    vec3 vertical(0.0f, 2.0f, 0.0f);
    vec3 origin(0.0f, 0.0f, 0.0f);
    
    
    for ( int j = ny-1; j >=0; --j )
    {
        for ( int i = 0; i < nx; ++i )
        {
            float u = float(i) / float(nx);
            float v = float(j) / float(ny);
            
            ray r(origin, lower_left_corner + u*horizontal + v*vertical);
            vec3 col = color(r);
            
            int ir = int(255.99f*col.r());
            int ig = int(255.99f*col.g());
            int ib = int(255.99f*col.b());
            std::cout << ir << " " << ig << " " << ib << "\n";
        }
    }
    
    return 0;
}
