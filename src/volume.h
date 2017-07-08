 #ifndef _RAYTRACER_VOLUME_H_
#define _RAYTRACER_VOLUME_H_
 
 struct constant_medium : public hitable
 {
     constant_medium( hitable *b, float d, texture *a ) 
         : boundary(b), density(d) 
     {
         phase_function = new isotropic(a);
     }
     
     virtual bool hit( const ray &r, float t_min, float t_max, hit_record &rec ) const override;
     virtual bool bounding_box( float t0, float t1, aabb &box ) const override
     {
         return boundary->bounding_box( t0, t1, box );
     }
     
     hitable *boundary;
     float density;
     material *phase_function;
 };
 
 bool constant_medium::hit( const ray &r, float t_min, float t_max, hit_record &rec ) const
 {
     hit_record rec1, rec2;
     
     // find intersect start of the volume
     if ( boundary->hit( r, -FLT_MAX, FLT_MAX, rec1 ) )
     {
         // find intersect end of the volume
         if ( boundary->hit( r, rec1.t + 0.0001f, FLT_MAX, rec2 ) )
         {
             if ( rec1.t < t_min ) rec1.t = t_min;
             if ( rec1.t > t_max ) rec1.t = t_max;
             if ( rec1.t >= rec2.t ) // no volume or too thin
             {
                 return false;
             }
             if ( rec1.t < 0 ) rec1.t = 0;
             
             float distance_inside_boundary = (rec2.t = rec1.t) * r.direction().length();
             // random hit distance inside the volume
             float hit_distance = -(1.0f/density)*std::logf(RAN01());
             rec.t = rec1.t + hit_distance / r.direction().length();
             rec.p = r.point_at_parameter( rec.t );
             rec.normal = vec3(1,0,0); // arbitrary. why?
             rec.mat_ptr = phase_function;
             return true;
         }
     }
     
     return false;
 }
 
#endif // _RAYTRACER_VOLUME_H_
 