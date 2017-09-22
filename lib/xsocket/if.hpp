#ifndef  _NET_BASE_IF_HPP_
#define  _NET_BASE_IF_HPP_

template< bool C_ > struct bool_
{
    static const bool value = C_;
    typedef bool_ type;
    typedef bool value_type;
    operator bool() const { return this->value; }
};

template<
      bool C,
      typename T1,
      typename T2
    >
struct if_c
{
    typedef T1 type;
};

template<
      typename T1,
      typename T2
    >
struct if_c<false,T1,T2>
{
    typedef T2 type;
};


template<
      typename T1,
      typename T2,
      typename T3
    >
struct if_
{
private:
    typedef if_c<
        T1::value,
        T2,
        T3
        > almost_type_;
 
 public:
    typedef typename almost_type_::type type;
    
};


#endif 

