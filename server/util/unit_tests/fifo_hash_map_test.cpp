#include "util/fifo_hash_map.h"
#include "gtest/gtest.h"
#include <tr1/functional>
#include <iostream>

using namespace std;
using namespace std::tr1;
using namespace std::tr1::placeholders;


class ValueClass
{
public:
	int32_t _val;
	int16_t _type;
	ValueClass(int32_t val,int16_t type) : _val(val),_type(type){} 
};

class TestClass
{
public:
	void destory(ValueClass** val,void* arg)
	{
		if(val && *val)
		{
			cout<< "destory val:" << (*val)->_val <<" type:"<< (*val)->_type << endl;
			delete *val;
			*val = NULL;
		}
	}

	void out_value(ValueClass** val,void*arg)
	{
		cout << "val:" << (*val)->_val << " type:" << (*val)->_type << endl;		
	}

	void time_out(ValueClass** val,void*arg, uint32_t & is_destory)
	{
		cout << "val:" << (*val)->_val << " type:" << (*val)->_type << endl;
		if((*val)->_type == 200)
		{
			is_destory = 1;
		}
		else
		{
			is_destory = 0;
		}		
	}

};

TEST(fifo_hash_map_test, set_get)
{
	TestClass test_obj;
	int32_t key = 1;
	FIFOHashmap<int32_t,ValueClass*> test_map(bind(&TestClass::destory,&test_obj,_1,_2));
	ValueClass* val = new ValueClass(1,200);
	test_map.set(key,val);

	ValueClass* val2 = NULL;
	EXPECT_EQ(test_map.get(key,val2),true);
	EXPECT_EQ(val2, val);
	
}

TEST(fifo_hash_map_test, foreach)
{
	TestClass test_obj;
	int32_t key = 1;
	FIFOHashmap<int32_t,ValueClass*> test_map(bind(&TestClass::destory,&test_obj,_1,_2));
	ValueClass* val = NULL;
    val = new ValueClass(1,200);	
	test_map.set(key++,val);
    val = new ValueClass(2,300);	
	test_map.set(key++,val);
    val = new ValueClass(3,400);	
	test_map.set(key++,val);
    val = new ValueClass(4,500);	
	test_map.set(key++,val);
	EXPECT_EQ(test_map.size(),key -1);
	test_map.foreach(bind(&TestClass::out_value,&test_obj,_1,_2),&test_obj);
}


TEST(fifo_hash_map_test, foreach_delete)
{
	TestClass test_obj;
	int32_t key = 1;
	FIFOHashmap<int32_t,ValueClass*> test_map(bind(&TestClass::destory,&test_obj,_1,_2));
	ValueClass* val = NULL;
    val = new ValueClass(1,200);	
	test_map.set(key++,val);
    val = new ValueClass(2,300);	
	test_map.set(key++,val);
    val = new ValueClass(3,400);	
	test_map.set(key++,val);
    val = new ValueClass(4,500);	
	test_map.set(key++,val);
	EXPECT_EQ(test_map.size(),key -1);
	test_map.foreach_delete(bind(&TestClass::time_out,&test_obj,_1,_2,_3),&test_obj);
	EXPECT_EQ(test_map.size(),key -2);

}


