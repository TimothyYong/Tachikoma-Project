#include <opencv2/highgui/highgui.hpp>
#include "Tensor.h"
#include "gpu_util.h"

Tensor::Tensor(void) {
  this->d_pixels = NULL;
  this->create(0, 0, 0, gfill::none);
}

Tensor::Tensor(const initializer_list<size_t> &shape, uint8_t fill_type) :
    shape(shape) {
  // create a new data field and initialize the size fields
  this->create(this->shape, fill_type);
}

Tensor::Tensor(const Tensor &gpucube) {
  this->d_pixels = NULL;
  this->copy(gpucube);
}

Tensor::Tensor(const Tensor *gpucube) {
  this->d_pixels = NULL;
  this->copy(*gpucube);
}

Tensor::~Tensor(void) {
  this->destroy();
}

__global__ void GPU_map_id(float *F, size_t n_elems) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n_elems) {
    return;
  }
  F[idx] = idx;
}

__global__ void GPU_map_assign(float *F, float val, size_t n_elems) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n_elems) {
    return;
  }
  F[idx] = val;
}

void Tensor::create(const initializer_list<size_t> &shape, uint8_t fill_type) {
  this->destroy();
  this->n_rows = n_rows;
  this->n_cols = n_cols;
  this->n_slices = n_slices;
  this->n_elem = n_rows * n_cols * n_slices;
  if (this->n_elem != 0) {
    checkCudaErrors(cudaMalloc(&this->d_pixels, this->n_elem * sizeof(float)));
    switch (fill_type) {
      case gfill::none:
        break;
      case gfill::zeros:
        checkCudaErrors(cudaMemset(this->d_pixels, 0, this->n_elem * sizeof(float)));
        break;
      case gfill::ones:
        GPU_map_assign<<<(this->n_elem-1) / 128 + 1, 128>>>(this->d_pixels, 1, this->n_elem);
        checkCudaErrors(cudaGetLastError());
        break;
      case gfill::linspace:
        GPU_map_id<<<(this->n_elem-1) / 128 + 1, 128>>>(this->d_pixels, this->n_elem);
        checkCudaErrors(cudaGetLastError());
      default:
        break;
    }
  }
}

void Tensor::destroy(void) {
  if (this->d_pixels) {
    checkCudaErrors(cudaFree(this->d_pixels));
    this->d_pixels = NULL;
  }
}

// OPERATORS

void Tensor::set(float v, size_t i, size_t j, size_t k) {
  checkCudaErrors(cudaMemcpy(&this->d_pixels[IJK2C(i, j, k, this->n_rows, this->n_cols)],
        &v, sizeof(float), cudaMemcpyHostToDevice));
}

float Tensor::get(size_t i, size_t j, size_t k) {
  float v;
  checkCudaErrors(cudaMemcpy(&v, &this->d_pixels[IJK2C(i, j, k, this->n_rows, this->n_cols)],
        sizeof(float), cudaMemcpyDeviceToHost));
  return v;
}

Tensor &Tensor::operator=(const Tensor &gpucube) {
  this->copy(gpucube);
  return *this;
}

// MEMORY

void Tensor::copy(const Tensor &gpucube) {
  this->create(gpucube.n_rows, gpucube.n_cols, gpucube.n_slices, gfill::none);
  checkCudaErrors(cudaMemcpy(this->d_pixels, gpucube.d_pixels,
        this->n_elem * sizeof(float), cudaMemcpyDeviceToDevice));
}

/*void Tensor::submatCopy(const Tensor &gpucube, int x1, int x2, int y1, int y2) {
  this->
}*/

void Tensor::load(const std::string &fname) { // change
  this->create(cv::imread(fname));
}

void Tensor::save(const std::string &fname) { // change
  cv::imwrite(fname, this->cv_img());
}

// Specific OpenCV interaction (to make sure that they are backwards compatible)

Tensor::Tensor(cv::Mat &cvMat) {
  this->d_pixels = NULL;
  this->create(cvMat);
}

__global__ void GPU_cv_img2Tensor(float *dst, unsigned char *src, int dst_n_rows, int dst_n_cols, int src_n_rows, int src_n_cols, int n_slices, int ioffset, int joffset) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  int j = blockIdx.y * blockDim.y + threadIdx.y;
  int k = blockIdx.z * blockDim.z + threadIdx.z;
  if (i >= dst_n_rows || j >= dst_n_cols || k >= n_slices) {
    return;
  }
  dst[IJK2C(i, j, n_slices-k-1, dst_n_rows, dst_n_cols)] = ((float)src[IJK2C(k, j+joffset, i+ioffset, n_slices, src_n_cols)]) / 255.0;
}

void Tensor::create(const cv::Mat &cvMat, bool remalloc) {
  if (remalloc) {
    this->create(cvMat.rows, cvMat.cols, cvMat.channels(), gfill::none);
  } else {
    assert(cvMat.rows * cvMat.cols * cvMat.channels() == this->n_elem && this->d_pixels != NULL);
  }
  if (this->n_elem == 0) {
    return;
  }
  // copy to memory
  unsigned char *dimg;
  checkCudaErrors(cudaMalloc(&dimg, sizeof(unsigned char) * this->n_elem));
  checkCudaErrors(cudaMemcpy(dimg, cvMat.data, sizeof(unsigned char) * this->n_elem, cudaMemcpyHostToDevice));

  // reformat
  dim3 blockSize(16, 16, 1);
  dim3 gridSize((this->n_rows-1)/16+1, (this->n_cols-1)/16+1, this->n_slices);
  GPU_cv_img2Tensor<<<gridSize, blockSize>>>(this->d_pixels, dimg, this->n_rows, this->n_cols, this->n_rows, this->n_cols, this->n_slices, 0, 0);
  checkCudaErrors(cudaGetLastError());
  checkCudaErrors(cudaFree(dimg));
}

void Tensor::create(const cv::Mat &cvMat, int i1, int i2, int j1, int j2, bool remalloc) {
  assert(i1 <= i2 && j1 <= j2 && j2 <= cvMat.cols && i2 <= cvMat.rows);
  int di = i2 - i1;
  int dj = j2 - j1;
  if (remalloc) {
    this->create(di, dj, cvMat.channels(), gfill::none);
  } else {
    assert(di * dj * cvMat.channels() == this->n_elem && this->d_pixels != NULL);
  }
  if (this->n_elem == 0) {
    return;
  }
  // copy to memory
  size_t n_elem = cvMat.rows * cvMat.cols * cvMat.channels();
  unsigned char *dimg;
  checkCudaErrors(cudaMalloc(&dimg, sizeof(unsigned char) * n_elem));
  checkCudaErrors(cudaMemcpy(dimg, cvMat.data, sizeof(unsigned char) * n_elem, cudaMemcpyHostToDevice));

  // reformat
  dim3 blockSize(16, 16, 1);
  dim3 gridSize((di-1)/16+1, (dj-1)/16+1, this->n_slices);
  GPU_cv_img2Tensor<<<gridSize, blockSize>>>(this->d_pixels, dimg, di, dj, cvMat.rows, cvMat.cols, this->n_slices, i1, j1);
  checkCudaErrors(cudaGetLastError());
  checkCudaErrors(cudaFree(dimg));
}

/*static int limit(int x, int a, int b) {
  if (x < a) {
    return a;
  } else if (x > b) {
    return b;
  } else {
    return x;
  }
}*/

__global__ void GPU_Tensor2cv_img(unsigned char *dst, float *src, int n_rows, int n_cols, int n_slices) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  int j = blockIdx.y * blockDim.y + threadIdx.y;
  int k = blockIdx.z * blockDim.z + threadIdx.z;
  if (i >= n_rows || j >= n_cols || k >= n_slices) {
    return;
  }
  dst[IJK2C(k, j, i, n_slices, n_cols)] = (unsigned char)(src[IJK2C(i, j, n_slices-k-1, n_rows, n_cols)] * 255.0);
}

cv::Mat Tensor::cv_img(void) {
  if (this->n_elem == 0) {
    return cv::Mat(0, 0, CV_8UC1);
  }
  cv::Mat cv_image(this->n_rows, this->n_cols, (this->n_slices == 3) ? CV_8UC3 : CV_8UC1);
  // reformat
  unsigned char *dimg;
  checkCudaErrors(cudaMalloc(&dimg, sizeof(unsigned char) * this->n_elem));
  dim3 blockSize(16, 16, 1);
  dim3 gridSize((this->n_rows-1)/16+1, (this->n_cols-1)/16+1, this->n_slices);
  GPU_Tensor2cv_img<<<gridSize, blockSize>>>(dimg, this->d_pixels, this->n_rows, this->n_cols, this->n_slices);
  checkCudaErrors(cudaGetLastError());

  // place the matrix into the image
  checkCudaErrors(cudaMemcpy(cv_image.data, dimg, sizeof(unsigned char) * this->n_elem, cudaMemcpyDeviceToHost));
  checkCudaErrors(cudaFree(dimg));
  return cv_image;
}

cv::Mat Tensor::cv_mat(void) {
  cv::Mat cv_image(this->n_rows, this->n_cols, (this->n_slices == 1 ? CV_32F : CV_32FC3));
  float *h_pixels = new float[this->n_elem];
  checkCudaErrors(cudaMemcpy(h_pixels, this->d_pixels,
        this->n_elem * sizeof(float), cudaMemcpyDeviceToHost));
  for (int i = 0; i < this->n_rows; i++) {
    for (int j = 0; j < this->n_cols; j++) {
      if (this->n_slices == 1) {
        cv_image.at<float>(i, j) = h_pixels[IJ2C(i, j, this->n_rows)];
      } else if (this->n_slices == 3) {
        cv_image.at<cv::Vec3f>(i, j) = cv::Vec3f(
              h_pixels[IJK2C(i, j, 0, this->n_rows, this->n_cols)],
              h_pixels[IJK2C(i, j, 1, this->n_rows, this->n_cols)],
              h_pixels[IJK2C(i, j, 2, this->n_rows, this->n_cols)]);
      }
    }
  }
  delete h_pixels;
  return cv_image;
}

// specific armadillo compatibility

Tensor::Tensor(arma::vec &armaCube) {
  this->d_pixels = NULL;
  this->create(armaCube);
}

Tensor::Tensor(arma::mat &armaCube) {
  this->d_pixels = NULL;
  this->create(armaCube);
}

Tensor::Tensor(arma::cube &armaCube) {
  this->d_pixels = NULL;
  this->create(armaCube);
}

void Tensor::create(const arma::vec &armaCube) {
  this->create(armaCube.n_rows, 1, 1, gfill::none);
  if (this->n_elem == 0) {
    return;
  }
  float *h_pixels = new float[this->n_elem];
  for (int i = 0; i < this->n_rows; i++) {
    h_pixels[i] = (float)armaCube(i);
  }
  checkCudaErrors(cudaMemcpy(this->d_pixels, h_pixels,
        this->n_elem * sizeof(float), cudaMemcpyHostToDevice));
  delete h_pixels;
}

void Tensor::create(const arma::mat &armaCube) {
  this->create(armaCube.n_rows, armaCube.n_cols, 1, gfill::none);
  if (this->n_elem == 0) {
    return;
  }
  float *h_pixels = new float[this->n_elem];
  for (int i = 0; i < this->n_rows; i++) {
    for (int j = 0; j < this->n_cols; j++) {
      h_pixels[IJ2C(i, j, this->n_rows)] = (float)armaCube(i, j);
    }
  }
  checkCudaErrors(cudaMemcpy(this->d_pixels, h_pixels,
        this->n_elem * sizeof(float), cudaMemcpyHostToDevice));
  delete h_pixels;
}

void Tensor::create(const arma::cube &armaCube) {
  this->create(armaCube.n_rows, armaCube.n_cols, armaCube.n_slices, gfill::none);
  if (this->n_elem == 0) {
    return;
  }
  float *h_pixels = new float[this->n_elem];
  for (int i = 0; i < this->n_rows; i++) {
    for (int j = 0; j < this->n_cols; j++) {
      for (int k = 0; k < this->n_slices; k++) {
        h_pixels[IJK2C(i, j, k, this->n_rows, this->n_cols)] = (float)armaCube(i, j, k);
      }
    }
  }
  checkCudaErrors(cudaMemcpy(this->d_pixels, h_pixels,
        this->n_elem * sizeof(float), cudaMemcpyHostToDevice));
  delete h_pixels;
}

arma::cube Tensor::arma_cube(void) {
  arma::cube ac(this->n_rows, this->n_cols, this->n_slices);
  float *h_pixels = new float[this->n_elem];
  checkCudaErrors(cudaMemcpy(h_pixels, this->d_pixels,
        this->n_elem * sizeof(float), cudaMemcpyDeviceToHost));
  for (int i = 0; i < this->n_rows; i++) {
    for (int j = 0; j < this->n_cols; j++) {
      for (int k = 0; k < this->n_slices; k++) {
        ac(i, j, k) = h_pixels[IJK2C(i, j, k, this->n_rows, this->n_cols)];
      }
    }
  }
  delete h_pixels;
  return ac;
}

/*Tensor &Tensor::operator=(const cv::Mat &cvMat) {
  this->create(cvMat);
  return *this;
}*/
