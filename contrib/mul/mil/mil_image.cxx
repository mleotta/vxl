#ifdef __GNUC__
#pragma implementation
#endif

//:
//  \file

#include <vcl_cstdlib.h>
#include <mil/mil_image.h>
#include <vsl/vsl_indent.h>
#include <vsl/vsl_binary_loader.h>

//=======================================================================

short mil_image::version_no() const
{
    return 1;
}

//=======================================================================

vcl_string mil_image::is_a() const
{
  static const vcl_string c_ = "mil_image";
  return c_;
}

//=======================================================================

bool mil_image::is_class(vcl_string const& s) const
{
  return s==mil_image::is_a();
}


//=======================================================================

void vsl_b_write(vsl_b_ostream& bfs, const mil_image& b)
{
    b.b_write(bfs);
}

//=======================================================================

void vsl_b_read(vsl_b_istream& bfs, mil_image& b)
{
    b.b_read(bfs);
}

//=======================================================================

vcl_ostream& operator<<(vcl_ostream& os,const mil_image& b)
{
    os << b.is_a() << ": ";
    vsl_indent_inc(os);
    b.print_summary(os);
    vsl_indent_dec(os);
    return os;
}

//=======================================================================

vcl_ostream& operator<<(vcl_ostream& os,const mil_image* b)
{
    if (b)
        return os << *b;
    else
        return os << "No mil_image defined.";
}

//: Print class to os
void vsl_print_summary(vcl_ostream& os, const mil_image& im)
{
  im.print_summary(os);
}

//=======================================================================

void vsl_add_to_binary_loader(const mil_image& b)
{
    vsl_binary_loader<mil_image>::instance().add(b);
}
