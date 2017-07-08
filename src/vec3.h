#ifndef _RAYTRACER_VEC3_H_
#define _RAYTRACER_VEC3_H_

struct vec3
{
    vec3() {};
    vec3(float e0, float e1, float e2) { e[0] = e0; e[1] = e1; e[2] = e2;}
    
    inline float x() const {return e[0];}
    inline float y() const {return e[1];}
    inline float z() const {return e[2];}
    inline float r() const {return e[0];}
    inline float g() const {return e[1];}
    inline float b() const {return e[2];}
    
    inline const vec3 &operator+() const {return *this;}
    inline vec3 operator-() const {return vec3(-e[0], -e[1], -e[2]);}
    inline float operator[]( int i ) const {return e[i];}
    inline float &operator[]( int i ) {return e[i];}
    
    inline vec3 &operator+=(const vec3 &rhs);
    inline vec3 &operator-=(const vec3 &rhs);
    inline vec3 &operator*=(const vec3 &rhs);
    inline vec3 &operator/=(const vec3 &rhs);
    inline vec3 &operator*=(float t);
    inline vec3 &operator/=(float t);
    
    inline float length() const { return sqrtf(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]); }
    inline float squared_length() const { return e[0]*e[0]+e[1]*e[1]+e[2]*e[2]; }
    inline void make_unit_vector();
    
    inline void clamp01();
    
    float e[3];
};

inline std::istream &operator>>(std::istream &is, vec3 &v)
{
    is >> v.e[0] >> v.e[1] >> v.e[2];
    return is;
}

inline std::ostream &operator<<(std::ostream &os, const vec3 &v)
{
    os << v.e[0] << " " << v.e[1] << " " << v.e[2];
    return os;
}

inline void vec3::make_unit_vector()
{
    float k = 1.0f / sqrtf(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);
    e[0] *= k;
    e[1] *= k;
    e[2] *= k;
}

inline vec3 operator+(const vec3 &v1, const vec3 &v2)
{
    return vec3(v1.e[0]+v2.e[0], v1.e[1]+v2.e[1], v1.e[2]+v2.e[2]);
}

inline vec3 operator-(const vec3 &v1, const vec3 &v2)
{
    return vec3(v1.e[0]-v2.e[0], v1.e[1]-v2.e[1], v1.e[2]-v2.e[2]);
}

inline vec3 operator*(const vec3 &v1, const vec3 &v2)
{
    return vec3(v1.e[0]*v2.e[0], v1.e[1]*v2.e[1], v1.e[2]*v2.e[2]);
}

inline vec3 operator/(const vec3 &v1, const vec3 &v2)
{
    return vec3(v1.e[0]/v2.e[0], v1.e[1]/v2.e[1], v1.e[2]/v2.e[2]);
}

inline vec3 operator*(const vec3 &v1, float t)
{
    return vec3(t*v1.e[0], t*v1.e[1], t*v1.e[2]);
}

inline vec3 operator/(const vec3 &v1, float t)
{
    return vec3(v1.e[0]/t, v1.e[1]/t, v1.e[2]/t);
}

inline vec3 operator*(float t, const vec3 &v1)
{
    return vec3(t*v1.e[0], t*v1.e[1], t*v1.e[2]);
}

inline vec3 operator/(float t, const vec3 &v1 )
{
    return vec3(v1.e[0]/t, v1.e[1]/t, v1.e[2]/t);
}

inline float dot( const vec3 &v1, const vec3 &v2)
{
    return v1.e[0]*v2.e[0]+v1.e[1]*v2.e[1]+v1.e[2]*v2.e[2];
}

inline vec3 cross(const vec3 &v1, const vec3 &v2)
{
    return vec3((v1.e[1]*v2.e[2] - v1.e[2]*v2.e[1]),
                (-(v1.e[0]*v2.e[2] - v1.e[2]*v2.e[0])),
                (v1.e[0]*v2.e[1] - v1.e[1]*v2.e[0]));
}

inline vec3 &vec3::operator+=(const vec3 &v)
{
    e[0] += v.e[0];
    e[1] += v.e[1];
    e[2] += v.e[2];
    return *this;
}

inline vec3 &vec3::operator-=(const vec3 &v)
{
    e[0] -= v.e[0];
    e[1] -= v.e[1];
    e[2] -= v.e[2];
    return *this;
}

inline vec3 &vec3::operator*=(const vec3 &v)
{
    e[0] *= v.e[0];
    e[1] *= v.e[1];
    e[2] *= v.e[2];
    return *this;
}

inline vec3 &vec3::operator/=(const vec3 &v)
{
    e[0] /= v.e[0];
    e[1] /= v.e[1];
    e[2] /= v.e[2];
    return *this;
}

inline vec3 &vec3::operator*=(float t)
{
    e[0] *= t;
    e[1] *= t;
    e[2] *= t;
    return *this;
}

inline vec3 &vec3::operator/=(float t)
{
    e[0] /= t;
    e[1] /= t;
    e[2] /= t;
    return *this;
}

inline vec3 unit_vector(vec3 v)
{
    return v / v.length();
}

inline void vec3::clamp01()
{
    for(int i=0;i<3;++i)
    {
        if (e[i] < 0.0f) e[i] = 0.0f;
        if (e[i] > 1.0f) e[i] = 1.0f;
    }
}

#endif // _RAYTRACER_VEC3_H_
