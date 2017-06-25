 #ifndef _RAYTRACER_RAY_H_
#define _RAYTRACER_RAY_H_
 
 struct ray
 {
     ray(){}
     ray( const vec3 &o, const vec3 &d, float t = 0.0f ) 
         : _origin(o), _direction(d), _time(t) {}
     
     vec3 origin() const { return _origin;}
     vec3 direction() const { return _direction;}
     float time() const { return _time; }
     
     vec3 point_at_parameter( float t ) const { return _origin + t * _direction; }
     
     vec3 _origin;
     vec3 _direction;
     float _time;
 };
 
#endif // _RAYTRACER_RAY_H_
 