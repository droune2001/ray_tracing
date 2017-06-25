 #ifndef _RAYTRACER_HITABLE_LIST_H_
#define _RAYTRACER_HITABLE_LIST_H_
 
 class hitable_list : public hitable
 {
     public:
     hitable_list() {}
     hitable_list( hitable **l, int n) : list(l), list_size(n) {}
     virtual bool hit( const ray &r, float t_min, float t_max, hit_record &rec) const override;
     virtual bool bounding_box( float t0, float t1, aabb &box ) const override;
     
     hitable ** list;
     int list_size;
 };
 
 bool hitable_list::hit( const ray &r, float t_min, float t_max, hit_record &rec) const
 {
     // returns the closest hit in a list
     hit_record temp_rec;
     bool hit_anything = false;
     float closest_so_far = t_max;
     for ( int i = 0; i < list_size; ++i )
     {
         if ( list[i]->hit(r, t_min, closest_so_far, temp_rec))
         {
             hit_anything = true;
             closest_so_far = temp_rec.t;
             rec = temp_rec;
         }
     }
     return hit_anything;
 }
 
 bool hitable_list::bounding_box( float t0, float t1, aabb &box ) const
 {
     if (list_size < 1) return false;
     aabb temp_box;
     bool first_true = list[0]->bounding_box(t0, t1, temp_box);
     if (!first_true) // early exit if forst is infinite plane?
     {
         return false;
     }
     else
     {
         box = temp_box;
     }
     
     for( int i = 1; i < list_size; ++i )
     {
         if (list[i]->bounding_box(t0, t1, temp_box))
         {
             // merge boxes, build the whole scene box
             box = surrounding_box(box, temp_box);
         }
         else
         {
             // if only one is infinite, then the list has no bounds.
             return false;
         }
     }
     
     return true;
 }
 
#endif // _RAYTRACER_HITABLE_LIST_H_
 