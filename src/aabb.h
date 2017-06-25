 #ifndef _RAYTRACER_AABB_H_
#define _RAYTRACER_AABB_H_
 
 inline float ffmin(float a, float b) { return a < b ? a : b; }
 inline float ffmax(float a, float b) { return a > b ? a : b; }
 
 struct aabb
 {
     aabb() {}
     aabb( const vec3 &a, const vec3 &b) : _min(a), _max(b) {}
     
     inline vec3 min() const { return _min; }
     inline vec3 max() const { return _max; }
     
     inline bool hit( const ray&r, float t_min, float t_max ) const;
     
     vec3 _min = vec3(FLT_MIN, FLT_MIN, FLT_MIN);
     vec3 _max = vec3(FLT_MAX, FLT_MAX, FLT_MAX);
 };
 
 aabb surrounding_box( aabb box0, aabb box1)
 {
     vec3 small(fmin(box0.min().x(), box1.min().x()),
                fmin(box0.min().y(), box1.min().y()),
                fmin(box0.min().z(), box1.min().z()));
     
     vec3 big(fmax(box0.max().x(), box1.max().x()),
              fmax(box0.max().y(), box1.max().y()),
              fmax(box0.max().z(), box1.max().z()));
     
     return aabb(small, big);
 }
 
 
#if 0
 inline bool aabb::hit( const ray&r, float t_min, float t_max ) const
 {
     // test the overlap of the 3 intervals (in 3 dimensions)
     for ( int a = 0; a < 3; ++a )
     {
         float intersect_with_min = ( _min[a] - r.origin()[a] ) / r.direction()[a];
         float intersect_with_max = ( _max[a] - r.origin()[a] ) / r.direction()[a];
         
         float t0 = ffmin( intersect_with_min,intersect_with_max );
         float t1 = ffmax( intersect_with_min,intersect_with_max );
         
         t_min = ffmax( t0, t_min );
         t_max = ffmin( t1, t_max );
         
         if ( t_max <= t_min )
         {
             return false;
         }
     }
     
     return true;
 }
#endif
 
 inline bool aabb::hit( const ray&r, float t_min, float t_max ) const
 {
     // test the overlap of the 3 intervals (in 3 dimensions)
     for ( int a = 0; a < 3; ++a )
     {
         float invD = 1.0f / r.direction()[a];
         float t0 = ( _min[a] - r.origin()[a] ) * invD;
         float t1 = ( _max[a] - r.origin()[a] ) * invD;
         if ( invD < 0.0f )
         {
             std::swap( t0, t1 );
         }
         
         t_min = t0 > t_min ? t0 : t_min;
         t_max = t1 < t_max ? t1 : t_max;
         
         if ( t_max <= t_min )
         {
             return false;
         }
     }
     
     return true;
 }
 
 
#endif // _RAYTRACER_AABB_H_
 