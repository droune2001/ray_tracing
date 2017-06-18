 #ifndef _RAYTRACER_SPHERE_H_
#define _RAYTRACER_SPHERE_H_
 
 class sphere : public hitable
 {
     public:
     
     sphere() {}
     sphere(vec3 cen, float r) : center(cen), radius(r) {}
     virtual bool hit(const ray &r, float t_min, float t_max, hit_record &rec ) const override;
     
     vec3 center;
     float radius;
 };
 
 bool sphere::hit(const ray &r, float t_min, float t_max, hit_record &rec) const
 {
     // NOTE(nfauvet): removed 4's and 2's that cancel each others out.
     vec3 oc = r.origin() - center;
     float a = dot( r.direction(), r.direction() );
     //float b = 2.0f * dot( oc, r.direction() );
     float b = dot( oc, r.direction() );
     float c = dot( oc, oc ) - radius * radius;
     //float discriminant = b*b - 4*a*c;
     float discriminant = b*b - a*c;
     if ( discriminant > 0.0f )
     {
         //float temp = (-b - sqrtf(discriminant)) / (2.0f*a);
         float temp = (-b - sqrtf(discriminant)) / a;
         if (temp < t_max && temp > t_min)
         {
             rec.t = temp;
             rec.p = r.point_at_parameter(rec.t);
             rec.normal = (rec.p - center) / radius;
             return true;
         }
         
         
         temp = (-b + sqrtf(discriminant)) / a;
         if (temp < t_max && temp > t_min)
         {
             rec.t = temp;
             rec.p = r.point_at_parameter(rec.t);
             rec.normal = (rec.p - center) / radius;
             return true;
         }
     }
     
     return false;
 }
 
#endif // _RAYTRACER_SPHERE_H_
 
 
