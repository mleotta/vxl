#include <vcl_cstdlib.h>
#include <vcl_iostream.h>
#include <vcl_cassert.h>
#include <vcl_cmath.h>

#include <vpgl/vpgl_calibration_matrix.h>

#include <vpgl/algo/vpgl_bundler_sfm_impl.h>
#include <vpgl/algo/vpgl_bundler_inters.h>
#include <vpgl/algo/vpgl_em_compute_5_point.h>

#include <vgl/vgl_point_2d.h>
#include <vgl/vgl_point_3d.h>

#include <vnl/vnl_matrix.h>
#include <vnl/vnl_vector.h>
#include <vnl/vnl_double_3x3.h>
#include <vnl/vnl_inverse.h>
#include <vnl/algo/vnl_svd.h>

// Generally useful function used for RANSAC.
// Randomly chooses n distinct indices into the set
static void get_distinct_indices(int n, int *idxs, int number_entries)
{
    for (int i = 0; i < n; i++) {
        bool found;
        int idx;

        do {
            found = true;
            idx = vcl_rand() % number_entries;

            for (int j = 0; j < i; j++) {
                if (idxs[j] == idx) {
                    found = false;
                    break;
                }
            }
        } while (!found);

        idxs[i] = idx;
    }
}


// Another generally useful function. Takes in a list of points and
// cameras, and finds the least-squared solution to the intersection
// of the rays generated by the points.
static double triangulate_points(
    const vcl_vector<vgl_point_2d<double> > &points,
    const vcl_vector<vpgl_perspective_camera<double> > &cameras,
    vgl_point_3d<double> &world_point)
{
    const int num_vars = 3;
    const int num_eqs = 2 * points.size();

    // Set up the least-squares solution.
    vnl_matrix<double> A(num_eqs, num_vars, 0.0);
    vnl_vector<double> b(num_eqs, 0.0);

    for (int i = 0; i < points.size(); i++) {
        const vgl_vector_3d<double> &trans =
            cameras[i].get_translation();

        const vnl_matrix_fixed<double, 3, 3> rot =
            cameras[i].get_rotation().as_matrix();

        // Set the row fo x for this point
        A.put(2 * i, 0, rot.get(0, 0) - points[i].x() * rot.get(2, 0) );
        A.put(2 * i, 1, rot.get(0, 1) - points[i].x() * rot.get(2, 1) );
        A.put(2 * i, 2, rot.get(0, 2) - points[i].x() * rot.get(2, 2) );

        // Set the row for y for this point
        A.put(2*i+1, 0,  rot.get(1, 0) - points[i].y() * rot.get(2, 0) );
        A.put(2*i+1, 1,  rot.get(1, 1) - points[i].y() * rot.get(2, 1) );
        A.put(2*i+1, 2,  rot.get(1, 2) - points[i].y() * rot.get(2, 2) );

        // Set the RHS row.
        b[2*i + 0] = trans.z() * points[i].x() - trans.x();
        b[2*i + 1] = trans.z() * points[i].y() - trans.y();
    }


    // Find the least squares result
    vnl_svd<double> svd(A);
    vnl_vector_fixed<double, 3> x = svd.solve(b);

    world_point.set(x.begin());

    // Find the error
    double error = 0.0;
    for (int i = 0; i < points.size(); i++) {
        // Compute projection error
        vnl_vector_fixed<double, 3> pp =
            cameras[i].get_rotation().as_matrix() * x;

        pp[0] += cameras[i].get_translation().x();
        pp[1] += cameras[i].get_translation().y();
        pp[2] += cameras[i].get_translation().z();

        double dx = pp[0] / pp[2] - points[i].x();
        double dy = pp[1] / pp[2] - points[i].y();
        error += dx * dx + dy * dy;
    }

    return vcl_sqrt(error / points.size());
}

/*-----------------------------------------------------------------------*/
// Takes in four matched points, and fills a 3x3 homography matrix
// Uses the direct linear transform method
void vpgl_bundler_sfm_impl_create_initial_recon::get_homography(
    const vcl_vector<vgl_point_2d<double> > &rhs,
    const vcl_vector<vgl_point_2d<double> > &lhs,
    vnl_double_3x3 &homography)
{
    assert(rhs.size() == 4);
    assert(lhs.size() == 4);

    vnl_matrix_fixed<double, 8, 8> A(0.0); // Left-hand matrix
    vnl_vector_fixed<double, 8> b(0.0); // Right-hand vector


    // Fill the right and left hand side. We are solving the equation Ax=b,
    // where x is the vectorized version of the homography,
    // b = [x_r1 y_r1 x_r2 y_r2 x_r3 y_r3 x_r4 y_r4]^T, and
    //
    // A = [x_l1 y_l1 1 0 0 0 -(x_l1*x_r1) -(y_l1*x_r1)]
    //     [0 0 0 x_l1 y_l1 1 -(x_l1*yr_1) -(y_l1*y_r1)]
    //     ...
    // For more information, look up the direct linear transform for
    // computing homographies
    for (int i = 0; i < 4; i++) {
        // Set the first row for this point
        A.put(2*i, 0, lhs[i].x());
        A.put(2*i, 1, lhs[i].y());
        A.put(2*i, 2, 1.0);

        A.put(2*i, 6, -lhs[i].x() * rhs[i].x());
        A.put(2*i, 7, -lhs[i].y() * rhs[i].x());

        // Set the second row for this point
        A.put(2*i+1, 3, lhs[i].x());
        A.put(2*i+1, 4, lhs[i].y());
        A.put(2*i+1, 5, 1.0);

        A.put(2*i+1, 6, -lhs[i].x() * rhs[i].y());
        A.put(2*i+1, 7, -lhs[i].y() * rhs[i].y());


        // Set the two rows in the B vector
        b.put(2*i, rhs[i].x());
        b.put(2*i+1, rhs[i].y());
    }

    // Solve the least squares problem
    vnl_svd<double> svd(A);
    vnl_vector<double> linear_homography = svd.solve(b);

    // Now transform the linearized homography into the square matrix version
    for (int i = 0; i < 8; i++) {
        homography.put( i / 3, i % 3, linear_homography.get(i));
    }

    homography.put(2, 2, 1);
}


// Estimates a homography and returns the percentage of inliers
double vpgl_bundler_sfm_impl_create_initial_recon::
    get_homography_inlier_percentage(
    const vpgl_bundler_inters_match_set &match)
{
    int inlier_count = 0;

    double threshold_squared =
        settings.inlier_threshold_homography *
        settings.inlier_threshold_homography;

    // RANSAC!
    for (int round = 0; round < settings.number_ransac_rounds_homography;
         round++)
    {
        int match_idxs[4];
        get_distinct_indices(4, match_idxs, match.num_features());

        // Fill these vectors with the points stored at the indices`
        vcl_vector<vgl_point_2d<double> > rhs;
        vcl_vector<vgl_point_2d<double> > lhs;

        for (int i = 0; i < 4; i++) {
            lhs.push_back(match.side1[match_idxs[i]]->point);
            rhs.push_back(match.side2[match_idxs[i]]->point);
        }

        // Get the homography for the points
        vnl_double_3x3 homography;
        get_homography(rhs, lhs, homography);


        // Count the number of inliers
        vnl_vector_fixed<double, 3> lhs_pt, rhs_pt;
        lhs_pt[2] = 1.0;

        int current_num_inliers = 0;

        vcl_vector<vpgl_bundler_inters_feature_sptr>::const_iterator s1, s2;
        for (s1 = match.side1.begin(), s2 = match.side2.begin();
             s1 != match.side1.end(); s1++, s2++)
        {
            lhs_pt[0] = (*s1)->point.x();
            lhs_pt[1] = (*s1)->point.y();

            rhs_pt = homography * lhs_pt;

            double dx = (rhs_pt[0] / rhs_pt[2]) - (*s2)->point.x();
            double dy = (rhs_pt[1] / rhs_pt[2]) - (*s2)->point.y();

            if (dx*dx + dy*dy <= threshold_squared) {
                current_num_inliers++;
            }
        }

        if (inlier_count < current_num_inliers) {
            inlier_count = current_num_inliers;
        }
    }

    return ((double) inlier_count) / match.side1.size();
}

// Returns the normalized image point from the image point "in"
vgl_point_2d<double> normalize_coords(const vgl_point_2d<double> &in,
                                      const vnl_matrix_fixed<double, 3, 3> &k_inv)
{
    vgl_point_2d<double> out;

    vnl_vector_fixed<double, 3> pt_in, pt_out;
    pt_in[0] = in.x(); pt_in[1] = in.y(); pt_in[2] = 1.0;

    pt_out = k_inv * pt_in;

    out.x() = pt_out[0] / pt_out[2];
    out.y() = pt_out[1] / pt_out[2];

    return out;
}

// Chooses two images from the set to create the initial reconstruction
void vpgl_bundler_sfm_impl_create_initial_recon::operator()(
    vpgl_bundler_inters_track_set &track_set,
    vpgl_bundler_inters_reconstruction &reconstruction)
{
    // First step: Find the two images to base the reconstruction on.
    const vpgl_bundler_inters_match_set *best_match = NULL;
    double lowest_inlier_percentage = 1.0;

    vcl_vector<vpgl_bundler_inters_match_set>::const_iterator ii;
    for (ii = track_set.match_sets.begin();
         ii != track_set.match_sets.end(); ii++)
    {
        // The pair must have a lot of matches, and have a focal length
        // from EXIF tags...
        if (ii->num_features()>=settings.min_number_of_matches_homography &&
            ii->image1.focal_length != VPGL_BUNDLER_NO_FOCAL_LEN &&
            ii->image1.focal_length != VPGL_BUNDLER_NO_FOCAL_LEN)
        {
            double current_inlier_percentage =
                get_homography_inlier_percentage(*ii);

            // ...but may not be modeled well by a homography, since this
            // means that there isn't a lot of 3D structure.
            if (current_inlier_percentage < lowest_inlier_percentage) {
                best_match = &(*ii);
                lowest_inlier_percentage = current_inlier_percentage;
            }
        }
    }

    if (not best_match) {
        vcl_cerr<<
            "Unable to create an initial reconstruction!\n" <<
            "There is not a match set that both has an initial guess " <<
            "from EXIF tags and at least " <<
            settings.min_number_of_matches_homography << " matches." <<
            '\n';

        vcl_exit(EXIT_FAILURE);
    }


    // We have the best match, so create two calibration matrices for
    // the image pair.
    vgl_point_2d<double> principal_point_1, principal_point_2;

    principal_point_1.x() = best_match->image1.source->ni() / 2.0;
    principal_point_1.y() = best_match->image1.source->nj() / 2.0;

    principal_point_2.x() = best_match->image2.source->ni() / 2.0;
    principal_point_2.y() = best_match->image2.source->nj() / 2.0;

    vpgl_calibration_matrix<double> k1(
        best_match->image1.focal_length, principal_point_1);

    vpgl_calibration_matrix<double> k2(
        best_match->image2.focal_length, principal_point_2);

    vnl_matrix_fixed<double, 3, 3> k1_inv = vnl_inverse(k1.get_matrix());
    vnl_matrix_fixed<double, 3, 3> k2_inv = vnl_inverse(k2.get_matrix());


    // Use the five-point algorithm wrapped in RANSAC to find the
    // relative pose.
    vpgl_essential_matrix<double> best_em;
    int best_inlier_count = 0;

    vpgl_em_compute_5_point<double> five_point;

    int match_idxs[5];
    for (int r = 0; r < settings.number_ransac_rounds_e_matrix; r++)
    {
        // Choose 5 random points, and use the 5-point algorithm on
        // these points to find the relative pose.
        vcl_vector<vgl_point_2d<double> > right_points, left_points;

        get_distinct_indices(5, match_idxs, best_match->num_features());
        for (int idx = 0; idx < 5; idx++) {
            right_points.push_back(
                normalize_coords(best_match->side1[idx]->point, k1_inv));

            left_points.push_back(
                normalize_coords(best_match->side2[idx]->point, k2_inv));
        }


        vcl_vector<vpgl_essential_matrix<double> > ems;
        five_point.compute(right_points, left_points, ems);


        // Now test all the essential matrices we've found, using them as
        // RANSAC hypotheses.
        vcl_vector<vpgl_essential_matrix<double> >::const_iterator i;
        for (i = ems.begin(); i != ems.end(); i++) {
            vpgl_fundamental_matrix<double> f(k1, k2, *i);

            vnl_matrix_fixed<double, 3, 1> point_r, point_l;

            // Count the number of inliers
            int inlier_count = 0;
            for (int j = 0; j < best_match->num_features(); j++) {
                point_r.put(0, 0, best_match->side1[j]->point.x());
                point_r.put(1, 0, best_match->side1[j]->point.y());
                point_r.put(2, 0, 1.0);

                point_l.put(0, 0, best_match->side2[j]->point.x());
                point_l.put(1, 0, best_match->side2[j]->point.y());
                point_l.put(2, 0, 1.0);

                if ( (point_l.transpose()*f.get_matrix()*point_r).get(0,0)
                    <= settings.inlier_threshold_e_matrix) {
                    inlier_count++;
                }
            }

            if (best_inlier_count < inlier_count) {
                best_em = *i;
            }
        }
    }

    // Get the two cameras.

    // Set the right camera to be have no translation or rotation.
    vgl_point_3d<double> camera_center_1(0, 0, 0);
    vgl_rotation_3d<double> rotation_1(0, 0, 0);
    vpgl_perspective_camera<double> right_camera(
        k1, camera_center_1, rotation_1);

    // Extract the left camera from the essential matrix
    vpgl_perspective_camera<double> left_camera;
    extract_left_camera(best_em,
                        normalize_coords(best_match->side2[0]->point, k2.get_matrix()),
                        normalize_coords(best_match->side1[0]->point, k1.get_matrix()),
                        left_camera);

    vpgl_bundler_inters_camera
        right_inters_camera(right_camera, best_match->image1),
        left_inters_camera(right_camera, best_match->image2);

    reconstruction.cameras.push_back(right_inters_camera);
    reconstruction.cameras.push_back(left_inters_camera);


    // Triangulate the points that both observe.
    for (int i = 0; i < best_match->num_features(); i++) {
        vpgl_bundler_inters_3d_point new_point;

        new_point.origins.push_back(best_match->side1[i]);
        new_point.origins.push_back(best_match->side2[i]);

        // Find the world point given these two image points
        // TODO

        // Add the point to the reconstruction
        reconstruction.points.push_back(new_point);
    }
}


/*-----------------------------------------------------------------------*/
static bool is_not_in_list(vpgl_bundler_inters_feature_set_sptr to_check,
                           const vcl_vector<vpgl_bundler_inters_camera> &cameras)
{
    vcl_vector<vpgl_bundler_inters_camera>::const_iterator ii;
    for (ii = cameras.begin(); ii != cameras.end(); ii++) {
        if (ii->image == to_check->source_image) return false;
    }

    return true;
}

// to_check is a collection of features, and a source image. This represents
// an unprocessed camera. We want to see how many points which we have
// already processed this camera observes. We look at every 3d point in
// the list, and TODO
static int count_observed_points(
    vpgl_bundler_inters_feature_set_sptr to_check,
    const vcl_vector<vpgl_bundler_inters_3d_point> &points)
{
    int num_observed_pts = 0;

    vcl_vector<vpgl_bundler_inters_3d_point>::const_iterator ii;
    for (ii = points.begin(); ii != points.end(); ii++) {
        // TODO
    }

    return num_observed_pts;
}

// Takes in reconstruction and track_set, fills to_add as a return val.
void vpgl_bundler_sfm_default_select_next_images::operator()(
    vpgl_bundler_inters_reconstruction &reconstruction,
    vpgl_bundler_inters_track_set &track_set,
    vcl_vector<vpgl_bundler_inters_feature_set_sptr> &to_add)
{
    // Look at every image
    int most_observed_points = 0;
    vpgl_bundler_inters_feature_set_sptr next_image;

    vcl_vector<vpgl_bundler_inters_feature_set_sptr>::iterator ii;
    for (ii = track_set.feature_sets.begin();
         ii != track_set.feature_sets.end(); ii++) {
        // Check to see if we've added this set before
        if (is_not_in_list(*ii, reconstruction.cameras)) {
            int currently_observed_points =
                count_observed_points(*ii, reconstruction.points);

            if (currently_observed_points > most_observed_points) {
                most_observed_points = currently_observed_points;
                next_image = *ii;
            }
        }
    }

    // Because we wanted this class to be very general, we need to return
    // a vector. Put the next image to add into the vector.
    to_add.push_back(next_image);
}

/*------------------------------------------------------------------------*/
// Adds to_add to the reconstruction.
void vpgl_bundler_sfm_default_add_next_images::operator()(
    vpgl_bundler_inters_reconstruction reconstruction,
    const vcl_vector<vpgl_bundler_inters_feature_set> &to_add) {
    // TODO
}


void vpgl_bundler_sfm_default_add_new_points::operator()(
    vpgl_bundler_inters_reconstruction &reconstruction,
    vpgl_bundler_inters_track_set &track_set) {
    // TODO
}

// Adjusts the reconstruction using nonlinear least squares
void vpgl_bundler_sfm_default_bundle_adjust::operator()(
    vpgl_bundler_inters_reconstruction reconstruction) {
    // TODO
}
