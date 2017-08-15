 #ifndef _RAYTRACER_HITABLE_H_
#define _RAYTRACER_HITABLE_H_
 
 struct material;
 
 struct hit_record
 {
     float t;
     float u, v;
     vec3 p;
     vec3 normal;
     material *mat_ptr;
 };
 
 struct hitable
 {
     virtual bool hit( const ray &r, float t_min, float t_max, hit_record &rec ) const = 0;
     virtual bool bounding_box( float t0, float t1, aabb &box ) const = 0;
     virtual float pdf_value( const vec3 &o, const vec3 &v ) const { return 0.0f; }
     virtual vec3 random( const vec3 &o ) const { return vec3(1,0,0); }
 };
 
 struct flip_normals : public hitable
 {
     flip_normals(hitable *h) : ptr(h) {}
     
     virtual bool hit( const ray &r, float t_min, float t_max, hit_record &rec ) const override
     {
         if (ptr->hit(r, t_min, t_max, rec))
         {
             rec.normal = -rec.normal;
             return true;
         }
         else
         {
             return false;
         }
     }
     
     virtual bool bounding_box( float t0, float t1, aabb &box ) const override
     {
         return ptr->bounding_box(t0, t1, box);
     }
     
     hitable *ptr;
 };
 
#endif // _RAYTRACER_HITABLE_H_
 
 
