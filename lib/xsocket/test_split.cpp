#include <stdio.h>
#include <stdlib.h>
#include "split_member.hpp"
#include "biniarchive.hpp"
#include "binoarchive.hpp"

class SplitMember
{
private:
    int _a;
    int _b;
    int _c;
public:
    SplitMember()
    :_a(0), _b(0), _c(0)
    {
    }
    SplitMember(int a, int b, int c)
    :_a(a), _b(b), _c(c)
    {
    }

    void dump()
    {
        printf("%d %d %d \n", _a, _b, _c);
    }
    
    ARCHIVE_SPLIT_MEMBER()
        
    template<class Archive>
    void load(Archive& ar)        
    {    
       ar&_a;
       ar&_b;
       ar&_c;
    }         

    template<class Archive>
    void save(Archive& ar)        
    {
        ar&_a;
        ar&_b;
        ar&_c;
    }       
};

int main()
{
    SplitMember split_o(1,2,3);
    char buf[1024];

    ByteBuffer bb(buf, 1024, 1024);

    BinOArchive oarchive(bb);
    oarchive&split_o;

    bb.flip();

    SplitMember split_i;
    BinIArchive iarchive(bb);
    iarchive&split_i;

    split_i.dump();   
}

