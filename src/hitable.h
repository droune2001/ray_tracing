 #ifndef _RAYTRACER_HITABLE_H_
#define _RAYTRACER_HITABLE_H_
 
 struct material;
 
 struct hit_record
 {
     float t;
     vec3 p;
     vec3 normal;
     material *mat_ptr;
 };
 
 class hitable
 {
     public:
     virtual bool hit( const ray &r, float t_min, float t_max, hit_record &rec ) const = 0;
 };
 
#endif // _RAYTRACER_HITABLE_H_
 
 
