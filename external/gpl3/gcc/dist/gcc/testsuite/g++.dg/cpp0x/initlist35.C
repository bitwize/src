// PR c++/41510
// { dg-options "-std=c++0x" }

struct B
{
  B(int, int);
};
struct A
{
  A(int, int);
  A(const B&);
};

void f()
{
  A a = { 1, 2 };
};

template <class T> void g()
{
  A a = { 1, 2 };
};

template void g<int>();
