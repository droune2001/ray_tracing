 #ifndef _RAYTRACER_HITABLE_H_
#define _RAYTRACER_HITABLE_H_
 
 struct hit_record
 {
     float t;
     vec3 p;
     vec3 normal;
 };
 
 class hitable
 {
     public:
     virtual bool hit(const ray &r, float t_min, float t_max, hit_record &rec ) const = 0;
 };
 
#endif // _RAYTRACER_HITABLE_H_
 
 
