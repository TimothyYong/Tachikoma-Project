#ifndef __TK_GCUBE_H__
#define __TK_GCUBE_H__

#ifdef __NVCC__

#include <opencv2/core/core.hpp>
#include <armadillo>
#include <string>
#include <initializer_list>

// TODO: allow compatibility with arma commands

namespace gfill {
  const uint8_t none = 0;
  const uint8_t zeros = 1;
  const uint8_t ones = 2;
  const uint8_t linspace = 3; // not implemented yet
};

class gcube {
  public:
    gcube(void);
    gcube(size_t n_rows,
          size_t n_cols = 1,
          size_t n_slices = 1,
          uint8_t fill_type = gfill::none);
    gcube(const std::initializer_list<float> &list); // add later for C++11 usage
    gcube(const std::initializer_list< std::initializer_list<float> > &list);
    gcube(const std::initializer_list< std::initializer_list< std::initializer_list<float> > > &list);
    ~gcube(void);

    void create(size_t n_rows,
                size_t n_cols = 1,
                size_t n_slices = 1,
                uint8_t fill_type = gfill::none);
    void create(const std::initializer_list<float> &list);
    void create(const std::initializer_list< std::initializer_list<float> > &list);
    void create(const std::initializer_list< std::initializer_list< std::initializer_list<float> > > &list);

    //gcube &operator=(const gcube &gpucube); // do not use, broken
    void copy(const gcube &gpucube);

    void load(const std::string &fname);
    void save(const std::string &fname);

    // opencv compatibility
    gcube(cv::Mat &cvMat);
    void create(const cv::Mat &cvMat);
    void create(const cv::Mat &cvMat, int x1, int x2, int y1, int y2);
    cv::Mat cv_img(void);
    gcube &operator=(const cv::Mat &cvMat);
    cv::Mat cv_mat(void);

    // armadillo compatibility
    arma::cube arma_cube(void);

    float *d_pixels;
    size_t n_rows;
    size_t n_cols;
    size_t n_slices;
    size_t n_elem;
};

#endif
#endif