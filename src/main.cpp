#include <iostream>
#include <cmath>
#include <cstdlib>
#include <float.h>

#include "vec3.h"
#include "ray.h"
#include "hitable.h"
#include "hitable_list.h"
#include "sphere.h"

vec3 color( const ray &r, hitable *world )
{
    hit_record rec;
    if (world->hit(r, 0.0f, FLT_MAX, rec))
    {
        return 0.5f*vec3(rec.normal.x()+1, rec.normal.y()+1, rec.normal.z()+1);
    }
    else
    {
        // BACKGROUND COLOR
        vec3 unit_direction = unit_vector(r.direction());
        // Y from [-1;1] to [0;1]
        float t = 0.5f * (unit_direction.y() + 1.0f);
        // gradient from blue to white
        return (1.0f - t) * vec3(1.0f, 1.0f, 1.0f) + t * vec3(0.5f, 0.7f, 1.0f);
    }
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
    
    hitable *list[2];
    list[0] = new sphere(vec3(0,0,-1), 0.5);
    list[1] = new sphere(vec3(0,-100.5,-1), 100);
    hitable *world = new hitable_list(list, 2);
    
    for ( int j = ny-1; j >=0; --j )
    {
        for ( int i = 0; i < nx; ++i )
        {
            float u = float(i) / float(nx);
            float v = float(j) / float(ny);
            
            ray r(origin, lower_left_corner + u*horizontal + v*vertical);
            vec3 col = color(r, world);
            
            int ir = int(255.99f*col.r());
            int ig = int(255.99f*col.g());
            int ib = int(255.99f*col.b());
            std::cout << ir << " " << ig << " " << ib << "\n";
        }
    }
    
    delete list[0];
    delete list[1];
    
    return 0;
}
