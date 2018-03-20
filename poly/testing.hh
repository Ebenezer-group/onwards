#pragma once
#include<Buffer.hh>

struct base{
    virtual ~base(){}
    static uint8_t const typeNum=1;

    base ()=default;
    template<class R>
    explicit base (::cmw::ReceiveBuffer<R>&);

    void MarshalMemberData (::cmw::SendBuffer&)const;
    void Marshal (::cmw::SendBuffer& b)const{MarshalMemberData(b);}
};

struct derived1:base{
    int32_t a;
    static uint8_t const typeNum=2;

    derived1 ()=default;
    template<class R>
    explicit derived1 (::cmw::ReceiveBuffer<R>&);

    void MarshalMemberData (::cmw::SendBuffer&)const;
    void Marshal (::cmw::SendBuffer& b)const{MarshalMemberData(b);}
};

struct derived2:base{
    int32_t b;
    static uint8_t const typeNum=3;

    derived2 ()=default;
    template<class R>
    explicit derived2 (::cmw::ReceiveBuffer<R>&);

    void MarshalMemberData (::cmw::SendBuffer&)const;
    void Marshal (::cmw::SendBuffer& b)const{MarshalMemberData(b);}
};

struct derived3:derived1{
    double c=3.14;
    static uint8_t const typeNum=4;

    derived3 ()=default;
    template<class R>
    explicit derived3 (::cmw::ReceiveBuffer<R>&);

    void MarshalMemberData (::cmw::SendBuffer&)const;
    void Marshal (::cmw::SendBuffer& b)const{MarshalMemberData(b);}
};

