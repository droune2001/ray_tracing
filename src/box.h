 #ifndef _RAYTRACER_BOX_H_
#define _RAYTRACER_BOX_H_
 
 struct box : public hitable
 {
     box() {}
     box( const vec3 &p0, const vec3 &p1, material *mat ) 
         : pmin(p0), pmax(p1)
     {
         hitable **list = new hitable*[6];
         list[0] = new xy_rect(p0.x(), p1.x(), p0.y(), p1.y(), p1.z(), mat);
         list[1] = new flip_normals(new xy_rect(p0.x(), p1.x(), p0.y(), p1.y(), p0.z(), mat));
         list[2] = new xz_rect(p0.x(), p1.x(), p0.z(), p1.z(), p1.y(), mat);
         list[3] = new flip_normals(new xz_rect(p0.x(), p1.x(), p0.z(), p1.z(), p0.y(), mat));
         list[4] = new yz_rect(p0.y(), p1.y(), p0.z(), p1.z(), p1.x(), mat);
         list[5] = new flip_normals(new yz_rect(p0.y(), p1.y(), p0.z(), p1.z(), p0.x(), mat));
         list_ptr = new hitable_list( list, 6 );
     }
     
     virtual bool hit( const ray &r, float t0, float t1, hit_record &rec ) const override
     {
         return list_ptr->hit(r,t0,t1,rec);
     }
     
     virtual bool bounding_box(float t0, float t1, aabb &box) const override
     {
         box = aabb(pmin, pmax);
         return true;
     }
     
     vec3 pmin, pmax;
     hitable *list_ptr;
 };
 
#endif // _RAYTRACER_BOX_H_
 