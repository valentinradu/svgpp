#pragma once

#include <boost/noncopyable.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/static_assert.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_floating_point.hpp>
#include <svgpp/policy/path.hpp>
#include <svgpp/definitions.hpp>
#include <svgpp/context_policy_load_path.hpp>
#include <svgpp/utility/arc_endpoint_to_center.hpp>
#include <svgpp/utility/arc_to_bezier.hpp>

namespace svgpp
{

BOOST_PARAMETER_TEMPLATE_KEYWORD(path_policy)

template<class Context>
struct context_policy<tag::path_policy, Context, void>: policy::path::default_policy
{};

template<class PathPolicy>
struct need_path_adapter: boost::mpl::bool_<
  PathPolicy::absolute_coordinates_only 
  || PathPolicy::no_ortho_line_to
  || PathPolicy::no_quadratic_bezier_shorthand 
  || PathPolicy::no_cubic_bezier_shorthand 
  || PathPolicy::quadratic_bezier_as_cubic>
{
};

namespace detail
{
  template<class Coordinate>
  struct path_adapter_cubic_bezier_data
  {
    typedef Coordinate coordinate_type;

    coordinate_type last_cubic_bezier_cp_x, last_cubic_bezier_cp_y;
    bool last_cubic_bezier_cp_valid;

    path_adapter_cubic_bezier_data()
      : last_cubic_bezier_cp_valid(false)
    {
    }

    void set_cubic_cp_base(coordinate_type absolute_x, coordinate_type absolute_y)
    {
      last_cubic_bezier_cp_x = absolute_x;
      last_cubic_bezier_cp_y = absolute_y;
      last_cubic_bezier_cp_valid = true;
    }

    void invalidate_last_cubic_bezier_cp()
    {
      last_cubic_bezier_cp_valid = false;
    }
  };

  template<class Coordinate>
  struct path_adapter_cubic_bezier_stub
  {
    typedef Coordinate coordinate_type;

    void set_cubic_cp_base(coordinate_type, coordinate_type)
    {
    }

    void invalidate_last_cubic_bezier_cp()
    {
    }
  };

  template<class Coordinate>
  struct path_adapter_quadratic_bezier_data
  {
    typedef Coordinate coordinate_type;

    coordinate_type last_quadratic_bezier_cp_x, last_quadratic_bezier_cp_y;
    bool last_quadratic_bezier_cp_valid;

    path_adapter_quadratic_bezier_data()
      : last_quadratic_bezier_cp_valid(false)
    {
    }

    void set_quadratic_cp_base(coordinate_type absolute_x, coordinate_type absolute_y)
    {
      last_quadratic_bezier_cp_x = absolute_x;
      last_quadratic_bezier_cp_y = absolute_y;
      last_quadratic_bezier_cp_valid = true;
    }

    void invalidate_last_quadratic_bezier_cp()
    {
      last_quadratic_bezier_cp_valid = false;
    }
  };

  template<class Coordinate>
  struct path_adapter_quadratic_bezier_stub
  {
    typedef Coordinate coordinate_type;

    void set_quadratic_cp_base(coordinate_type, coordinate_type)
    {
    }

    void invalidate_last_quadratic_bezier_cp()
    {
    }
  };
}

template<
  class OutputContext, 
  class PathPolicy = context_policy<tag::path_policy, OutputContext>, 
  class Coordinate = typename context_policy<tag::number_type, OutputContext>::type,
  class LoadPolicy = context_policy<tag::load_path_policy, OutputContext> >
class path_adapter: 
  boost::noncopyable,
  private boost::mpl::if_c<PathPolicy::no_cubic_bezier_shorthand, 
    detail::path_adapter_cubic_bezier_data<Coordinate>,
    detail::path_adapter_cubic_bezier_stub<Coordinate>
  >::type,
  private boost::mpl::if_c<PathPolicy::no_quadratic_bezier_shorthand || PathPolicy::quadratic_bezier_as_cubic, 
    detail::path_adapter_quadratic_bezier_data<Coordinate>,
    detail::path_adapter_quadratic_bezier_stub<Coordinate>
  >::type
{
public:
  typedef Coordinate coordinate_type;

  explicit path_adapter(OutputContext & original_context_)
    : output_context(original_context_)
    , current_x(0), current_y(0)
    , subpath_start_x(0), subpath_start_y(0)
  {
    BOOST_STATIC_ASSERT_MSG(!PathPolicy::quadratic_bezier_as_cubic || PathPolicy::no_cubic_bezier_shorthand, "First option requires second one");
  }

private:
  void non_curve_command()
  {
    this->invalidate_last_cubic_bezier_cp();
    this->invalidate_last_quadratic_bezier_cp();
  }

  void set_cubic_cp(coordinate_type absolute_x, coordinate_type absolute_y)
  {
    this->set_cubic_cp_base(absolute_x, absolute_y);
    this->invalidate_last_quadratic_bezier_cp();
  }

  void set_quadratic_cp(coordinate_type absolute_x, coordinate_type absolute_y)
  {
    this->set_quadratic_cp_base(absolute_x, absolute_y);
    this->invalidate_last_cubic_bezier_cp();
  }

  OutputContext & output_context;
  // TODO: move some data to optional base class
  coordinate_type current_x, current_y, subpath_start_x, subpath_start_y;

public:
  OutputContext & get_output_context() const { return output_context; }

  template<class Policy>
  void path_move_to(
    coordinate_type x, 
    coordinate_type y, tag::absolute_coordinate tag)
  { 
    LoadPolicy::path_move_to(output_context, x, y, tag);
    non_curve_command();
    current_x = x;
    current_y = y;
    subpath_start_x = x;
    subpath_start_y = y;
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::absolute_coordinates_only>::type
  path_move_to(
    coordinate_type x, 
    coordinate_type y, tag::relative_coordinate)
  { 
    current_x += x;
    current_y += y;
    path_move_to<Policy>(current_x, current_y, tag::absolute_coordinate());
    non_curve_command();
    subpath_start_x = current_x;
    subpath_start_y = current_y;
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::absolute_coordinates_only>::type
  path_move_to(
    coordinate_type x, 
    coordinate_type y, tag::relative_coordinate tag)
  { 
    current_x += x;
    current_y += y;
    path_move_to<Policy>(x, y, tag);
    non_curve_command();
    subpath_start_x = current_x;
    subpath_start_y = current_y;
  }

  template<class Policy>
  void path_line_to(
    coordinate_type x, 
    coordinate_type y, tag::absolute_coordinate tag)
  { 
    current_x = x;
    current_y = y;
    LoadPolicy::path_line_to(output_context, x, y, tag);
    non_curve_command();
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::absolute_coordinates_only>::type
  path_line_to(
    coordinate_type x, 
    coordinate_type y, tag::relative_coordinate)
  {
    current_x += x;
    current_y += y;
    LoadPolicy::path_line_to(output_context, current_x, current_y, tag::absolute_coordinate());
    non_curve_command();
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::absolute_coordinates_only>::type
  path_line_to(
    coordinate_type x, 
    coordinate_type y, tag::relative_coordinate tag)
  {
    current_x += x;
    current_y += y;
    LoadPolicy::path_line_to(output_context, x, y, tag);
    non_curve_command();
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::no_ortho_line_to>::type
  path_line_to_ortho(
    coordinate_type coord, 
    bool horizontal, tag::relative_coordinate tag)
  {
    if (horizontal)
      path_line_to<Policy>(coord, 0, tag);
    else
      path_line_to<Policy>(0, coord, tag);
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::no_ortho_line_to>::type
  path_line_to_ortho(
    coordinate_type coord, 
    bool horizontal, tag::absolute_coordinate tag)
  {
    if (horizontal)
      path_line_to<Policy>(coord, current_y, tag);
    else
      path_line_to<Policy>(current_x, coord, tag);
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::no_ortho_line_to>::type
  path_line_to_ortho(
    coordinate_type coord, bool horizontal, tag::absolute_coordinate tag)
  {
    if (horizontal)
      current_x = coord;
    else
      current_y = coord;
    LoadPolicy::path_line_to_ortho(output_context, coord, horizontal, tag);
    non_curve_command();
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::absolute_coordinates_only && !Policy::no_ortho_line_to>::type
  path_line_to_ortho(
    coordinate_type coord, bool horizontal, tag::relative_coordinate tag)
  {
    if (horizontal)
      current_x += coord;
    else
      current_y += coord;
    LoadPolicy::path_line_to_ortho(output_context, 
      horizontal ? current_x : current_y, horizontal, tag::absolute_coordinate());
    non_curve_command();
  }

  template<class Policy>
  typename boost::enable_if_c<!Policy::absolute_coordinates_only && !Policy::no_ortho_line_to>::type
  path_line_to_ortho(
    coordinate_type coord, bool horizontal, tag::relative_coordinate tag)
  {
    if (horizontal)
      current_x += coord;
    else
      current_y += coord;
    LoadPolicy::path_line_to_ortho(output_context, coord, horizontal, tag);
    non_curve_command();
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  {
    current_x = x;
    current_y = y;
    LoadPolicy::path_quadratic_bezier_to(output_context, x1, y1, x, y, tag);
    set_quadratic_cp(x1, y1);
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::absolute_coordinates_only && !Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate)
  {
    x += current_x;
    y += current_y;
    x1 += current_x;
    y1 += current_y;
    current_x = x;
    current_y = y;
    LoadPolicy::path_quadratic_bezier_to(output_context, x1, y1, x, y, tag::absolute_coordinate());
    set_quadratic_cp(x1, y1);
  }

  template<class Policy>
  typename boost::enable_if_c<!Policy::absolute_coordinates_only && !Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    LoadPolicy::path_quadratic_bezier_to(output_context, x1, y1, x, y, tag);
    set_quadratic_cp(x1 + current_x, y1 + current_y);
    current_x += x;
    current_y += y;
  }

  template<class Policy>
  typename boost::enable_if_c<
                              Policy::no_quadratic_bezier_shorthand
                              || Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  {
    if (this->last_quadratic_bezier_cp_valid)
      path_quadratic_bezier_to<Policy>(
        2 * current_x - this->last_quadratic_bezier_cp_x,
        2 * current_y - this->last_quadratic_bezier_cp_y, 
        x, y, tag);
    else
      path_quadratic_bezier_to<Policy>(current_x, current_y, x, y, tag);
  }

  template<class Policy>
  typename boost::enable_if_c<
                              Policy::no_quadratic_bezier_shorthand
                              || Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    if (this->last_quadratic_bezier_cp_valid)
      path_quadratic_bezier_to<Policy>(
        current_x - this->last_quadratic_bezier_cp_x,
        current_y - this->last_quadratic_bezier_cp_y, x, y, tag);
    else
      path_quadratic_bezier_to<Policy>(0, 0, x, y, tag);
  }

  template<class Policy>
  typename boost::disable_if_c<
                                Policy::no_quadratic_bezier_shorthand
                                || Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  {
    LoadPolicy::path_quadratic_bezier_to(output_context, x, y, tag);
    if (this->last_quadratic_bezier_cp_valid)
      set_quadratic_cp(
        2 * current_x - this->last_quadratic_bezier_cp_x,
        2 * current_y - this->last_quadratic_bezier_cp_y);
    else
      set_quadratic_cp(current_x, current_y);
    current_x = x;
    current_y = y;
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::absolute_coordinates_only
    && !(Policy::no_quadratic_bezier_shorthand
      || Policy::quadratic_bezier_as_cubic)>::type
  path_quadratic_bezier_to(
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    x += current_x;
    y += current_y;
    LoadPolicy::path_quadratic_bezier_to(output_context, x, y, tag::absolute_coordinate());
    if (this->last_quadratic_bezier_cp_valid)
      set_quadratic_cp(
        2 * current_x - this->last_quadratic_bezier_cp_x,
        2 * current_y - this->last_quadratic_bezier_cp_y);
    else
      set_quadratic_cp(current_x, current_y);
    current_x = x;
    current_y = y;
  }

  template<class Policy>
  typename boost::enable_if_c<!Policy::absolute_coordinates_only
    && !(Policy::no_quadratic_bezier_shorthand
    || Policy::quadratic_bezier_as_cubic)>::type
  path_quadratic_bezier_to(
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    LoadPolicy::path_quadratic_bezier_to(output_context, x, y, tag);
    if (this->last_quadratic_bezier_cp_valid)
      set_quadratic_cp(
      2 * current_x - this->last_quadratic_bezier_cp_x,
      2 * current_y - this->last_quadratic_bezier_cp_y);
    else
      set_quadratic_cp(current_x, current_y);
    current_x += x;
    current_y += y;
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  {
    BOOST_STATIC_ASSERT(boost::is_floating_point<coordinate_type>::value);
    const coordinate_type k1 = 1./3.;
    const coordinate_type k2 = 2./3.;
    coordinate_type xk = k2 * x1;
    coordinate_type yk = k2 * y1;
    path_cubic_bezier_to<Policy>(
      current_x * k1 + xk, current_y * k1 + yk,
      x * k1 + xk, y * k1 + yk,
      x, y, tag);
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::quadratic_bezier_as_cubic>::type
  path_quadratic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    BOOST_STATIC_ASSERT(boost::is_floating_point<coordinate_type>::value);
    const coordinate_type k1 = 1./3.;
    const coordinate_type k2 = 2./3.;
    coordinate_type xk = k2 * x1;
    coordinate_type yk = k2 * y1;
    path_cubic_bezier_to<Policy>(
      xk, yk,
      x * k1 + xk, y * k1 + yk,
      x, y, tag);
  }

  template<class Policy>
  void path_cubic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x2, 
    coordinate_type y2, 
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  {
    current_x = x;
    current_y = y;
    LoadPolicy::path_cubic_bezier_to(output_context, x1, y1, x2, y2, x, y, tag);
    set_cubic_cp(x2, y2);
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::absolute_coordinates_only>::type
  path_cubic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x2, 
    coordinate_type y2, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate)
  {
    x += current_x;
    y += current_y;
    x1 += current_x;
    y1 += current_y;
    x2 += current_x;
    y2 += current_y;
    LoadPolicy::path_cubic_bezier_to(output_context, x1, y1, x2, y2, x, y, tag::absolute_coordinate());
    set_cubic_cp(x2, y2);
    current_x = x;
    current_y = y;
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::absolute_coordinates_only>::type
  path_cubic_bezier_to(
    coordinate_type x1, 
    coordinate_type y1, 
    coordinate_type x2, 
    coordinate_type y2, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    LoadPolicy::path_cubic_bezier_to(output_context, x1, y1, x2, y2, x, y, tag);
    set_cubic_cp(x2 + current_x, y2 + current_y);
    current_x += x;
    current_y += y;
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::no_cubic_bezier_shorthand>::type
  path_cubic_bezier_to(
      coordinate_type x2, 
      coordinate_type y2, 
      coordinate_type x, 
      coordinate_type y, 
      tag::absolute_coordinate tag)
  {
    if (this->last_cubic_bezier_cp_valid)
      path_cubic_bezier_to<Policy>( 
        2 * current_x - this->last_cubic_bezier_cp_x,
        2 * current_y - this->last_cubic_bezier_cp_y, 
        x2, y2, x, y, tag);
    else
      path_cubic_bezier_to<Policy>(current_x, current_y, x2, y2, x, y, tag);
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::no_cubic_bezier_shorthand>::type
  path_cubic_bezier_to(
    coordinate_type x2, 
    coordinate_type y2, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    if (this->last_cubic_bezier_cp_valid)
      path_cubic_bezier_to<Policy>(
        current_x - this->last_cubic_bezier_cp_x,
        current_y - this->last_cubic_bezier_cp_y, x2, y2, x, y, tag);
    else
      path_cubic_bezier_to<Policy>(0, 0, x2, y2, x, y, tag);
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::no_cubic_bezier_shorthand>::type
  path_cubic_bezier_to(
    coordinate_type x2, 
    coordinate_type y2, 
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  {
    current_x = x;
    current_y = y;
    LoadPolicy::path_cubic_bezier_to(output_context, x2, y2, x, y, tag);
    set_cubic_cp(x2, y2);
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::no_cubic_bezier_shorthand>::type
  path_cubic_bezier_to(
    coordinate_type x2, 
    coordinate_type y2, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  {
    LoadPolicy::path_cubic_bezier_to(output_context, x2, y2, x, y, tag);
    set_cubic_cp(current_x + x2, current_y + y2);
    current_x += x;
    current_y += y;
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::arc_as_cubic_bezier>::type
  path_elliptical_arc_to(
    coordinate_type rx, 
    coordinate_type ry, 
    coordinate_type x_axis_rotation, 
    bool large_arc_flag, bool sweep_flag, 
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  { 
    coordinate_type cx, cy, theta1, theta2;
    arc_endpoint_to_center(current_x, current_y, x, y,
      rx, ry, x_axis_rotation, large_arc_flag, sweep_flag,
      cx, cy, theta1, theta2);
    if (sweep_flag)
    {
      if (theta2 < theta1)
        theta2 += boost::math::constants::two_pi<coordinate_type>();
    }
    else
    {
      if (theta2 > theta1)
        theta2 -= boost::math::constants::two_pi<coordinate_type>();
    }

    typedef arc_to_bezier<coordinate_type> arc_to_bezier_t;
    arc_to_bezier_t a2b(cx, cy, rx, ry, x_axis_rotation, 
      arc_to_bezier_t::circle_angle_tag(), theta1, theta2, 
      arc_to_bezier_t::max_angle_tag(), boost::math::constants::half_pi<coordinate_type>());
    for(arc_to_bezier_t::iterator it(a2b); !it.eof(); it.advance())
      path_cubic_bezier_to<Policy>(
        it.p1x(), it.p1y(),
        it.p2x(), it.p2y(),
        it.p3x(), it.p3y(),
        tag::absolute_coordinate());
  } 

  template<class Policy>
  typename boost::disable_if_c<Policy::arc_as_cubic_bezier>::type
  path_elliptical_arc_to(
    coordinate_type rx, 
    coordinate_type ry, 
    coordinate_type x_axis_rotation, 
    bool large_arc_flag, bool sweep_flag, 
    coordinate_type x, 
    coordinate_type y, 
    tag::absolute_coordinate tag)
  { 
    LoadPolicy::path_elliptical_arc_to(output_context, rx, ry, x_axis_rotation, large_arc_flag, sweep_flag, x, y, tag); 
    non_curve_command();
    current_x = x;
    current_y = y;
  }

  template<class Policy>
  typename boost::enable_if_c<Policy::absolute_coordinates_only || Policy::arc_as_cubic_bezier>::type
  path_elliptical_arc_to(
    coordinate_type rx, 
    coordinate_type ry, 
    coordinate_type x_axis_rotation, 
    bool large_arc_flag, bool sweep_flag, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate)
  { 
    path_elliptical_arc_to<Policy>( 
      rx, ry, x_axis_rotation, large_arc_flag, sweep_flag, x + current_x, y + current_y, tag::absolute_coordinate()); 
  }

  template<class Policy>
  typename boost::disable_if_c<Policy::absolute_coordinates_only || Policy::arc_as_cubic_bezier>::type
  path_elliptical_arc_to(
    coordinate_type rx, 
    coordinate_type ry, 
    coordinate_type x_axis_rotation, 
    bool large_arc_flag, bool sweep_flag, 
    coordinate_type x, 
    coordinate_type y, 
    tag::relative_coordinate tag)
  { 
    LoadPolicy::path_elliptical_arc_to(output_context, 
      rx, ry, x_axis_rotation, large_arc_flag, sweep_flag, x, y, tag); 
    non_curve_command();
    current_x += x;
    current_y += y;
  }

  template<class Policy>
  void path_close_subpath()
  {
    LoadPolicy::path_close_subpath(output_context);
    non_curve_command();
    current_x = subpath_start_x;
    current_y = subpath_start_y;
  }

  template<class Policy>
  void path_exit()
  {
    LoadPolicy::path_exit(output_context);
  }
};

namespace detail
{

// This class created just to pass PathPolicy as an template argument to
// path_adapter methods so that boost::enable_if can be used
template<
  class Adapter, 
  class PathPolicy, 
  class Coordinate
>
struct path_adapter_load_path_policy
{
  template<class AbsoluteOrRelative>
  static void path_move_to(Adapter & context, Coordinate x, Coordinate y, AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_move_to<PathPolicy>(x, y, absoluteOrRelative); 
  }

  template<class AbsoluteOrRelative>
  static void path_line_to(Adapter & context, Coordinate x, Coordinate y, AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_line_to<PathPolicy>(x, y, absoluteOrRelative); 
  }

  template<class AbsoluteOrRelative>
  static void path_line_to_ortho(Adapter & context, Coordinate coord, bool horizontal, AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_line_to_ortho<PathPolicy>(coord, horizontal, absoluteOrRelative); 
  }

  template<class AbsoluteOrRelative>
  static void path_cubic_bezier_to(Adapter & context, Coordinate x1, Coordinate y1, 
    Coordinate x2, Coordinate y2, 
    Coordinate x, Coordinate y, 
    AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_cubic_bezier_to<PathPolicy>(x1, y1, x2, y2, x, y, absoluteOrRelative); 
  }

  template<class AbsoluteOrRelative>
  static void path_cubic_bezier_to(Adapter & context, 
    Coordinate x2, Coordinate y2, 
    Coordinate x, Coordinate y, 
    AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_cubic_bezier_to<PathPolicy>(x2, y2, x, y, absoluteOrRelative); 
  }

  template<class AbsoluteOrRelative>
  static void path_quadratic_bezier_to(Adapter & context, 
    Coordinate x1, Coordinate y1, 
    Coordinate x, Coordinate y, 
    AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_quadratic_bezier_to<PathPolicy>(x1, y1, x, y, absoluteOrRelative); 
  }

  template<class AbsoluteOrRelative>
  static void path_quadratic_bezier_to(Adapter & context, 
    Coordinate x, Coordinate y, 
    AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_quadratic_bezier_to<PathPolicy>(x, y, absoluteOrRelative); 
  }

  template<class AbsoluteOrRelative>
  static void path_elliptical_arc_to(Adapter & context, 
    Coordinate rx, Coordinate ry, Coordinate x_axis_rotation,
    bool large_arc_flag, bool sweep_flag, 
    Coordinate x, Coordinate y,
    AbsoluteOrRelative absoluteOrRelative)
  { 
    context.path_elliptical_arc_to<PathPolicy>(rx, ry, x_axis_rotation, large_arc_flag, sweep_flag, x, y, absoluteOrRelative); 
  }

  static void path_close_subpath(Adapter & context)
  { 
    context.template path_close_subpath<PathPolicy>(); 
  }

  static void path_exit(Adapter & context)
  { 
    context.template path_exit<PathPolicy>(); 
  }
};

template<
  class OutputContext, 
  class PathPolicy   = context_policy<tag::path_policy, OutputContext>, 
  class Coordinate   = typename context_policy<tag::number_type, OutputContext>::type,
  class LoadPolicy   = context_policy<tag::load_path_policy, OutputContext>,
  class Enabled = void>
struct path_adapter_if_needed
{
  typedef OutputContext type;
  typedef type & holder_type;
  typedef LoadPolicy load_path_policy;

  static OutputContext & get_original_context(holder_type & adapted_context)
  {
    return adapted_context;
  }
};

template<
  class OutputContext, 
  class PathPolicy, 
  class Coordinate,
  class LoadPolicy>
struct path_adapter_if_needed<OutputContext, PathPolicy, Coordinate, LoadPolicy, 
  typename boost::enable_if<need_path_adapter<PathPolicy> >::type>
{
  typedef path_adapter<OutputContext, PathPolicy, Coordinate, LoadPolicy> type;
  typedef type holder_type;
  typedef detail::path_adapter_load_path_policy<type, PathPolicy, Coordinate> load_path_policy;

  static OutputContext & get_original_context(holder_type & adapted_context)
  {
    return adapted_context.get_output_context();
  }
};

}

}