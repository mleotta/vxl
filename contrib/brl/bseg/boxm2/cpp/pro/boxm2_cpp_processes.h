#ifndef boxm2_cpp_processes_h_
#define boxm2_cpp_processes_h_

#include <bprb/bprb_func_process.h>
#include <bprb/bprb_macros.h>

//the init functions
DECLARE_FUNC_CONS(boxm2_cpp_render_expected_image_process);
DECLARE_FUNC_CONS(boxm2_cpp_render_cone_expected_image_process);
DECLARE_FUNC_CONS(boxm2_cpp_update_image_process);
DECLARE_FUNC_CONS(boxm2_cpp_cone_update_image_process);
DECLARE_FUNC_CONS(boxm2_cpp_refine_process2);
DECLARE_FUNC_CONS(boxm2_cpp_change_detection_process2);
DECLARE_FUNC_CONS(boxm2_cpp_query_cell_data_process);
DECLARE_FUNC_CONS(boxm2_cpp_render_expected_image_process);
DECLARE_FUNC_CONS(boxm2_cpp_cast_intensities_process);
DECLARE_FUNC_CONS(boxm2_cpp_mean_intensities_batch_process);
DECLARE_FUNC_CONS(boxm2_cpp_mean_intensities_print_process);
DECLARE_FUNC_CONS(boxm2_cpp_create_norm_intensities_process);
DECLARE_FUNC_CONS(boxm2_cpp_data_print_process);
DECLARE_FUNC_CONS(boxm2_cpp_filter_process);

DECLARE_FUNC_CONS(boxm2_cpp_vis_of_point_process);
DECLARE_FUNC_CONS(boxm2_cpp_ray_probe_process);
DECLARE_FUNC_CONS(boxm2_cpp_ray_app_density_process);
DECLARE_FUNC_CONS(boxm2_cpp_create_aux_data_process);
DECLARE_FUNC_CONS(boxm2_cpp_batch_update_process);

#endif
