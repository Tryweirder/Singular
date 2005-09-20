//$Id: ring_wrap.cc,v 1.2 2005-09-20 08:39:44 bricken Exp $
#include <poly_wrap.h>
#include <boost/python.hpp>
#include "mod2.h"
#include "ring_wrap.h"
static boost::python::object Ring_as_str(const Ring& r)
{
  using boost::python::str;
  StringSetS("");
  rWrite(r.pimpl);
  char* out=StringAppendS("");
  return boost::python::str(out,strlen(out));
}
void export_ring(){
boost::python::class_<Ring>("ring")
     .def("__str__", Ring_as_str)
     .def(boost::python::init <>());

}