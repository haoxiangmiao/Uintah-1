/* Some useful commands

unu permute -i ../sphere00000.nrrd -p 0 3 1 2 | unu reshape -s 24 64 64 -o group.nrrd

./pca-image -i group.nrrd -numbases 1 -o munged1

unu reshape -i munged1.nrrd -s 3 8 64 64 | unu permute -p 0 2 3 1 | unu reshape -s 3 64 64 2 4 | unu permute -p 0 1 3 2 4 | unu reshape -s 3 128 256 | XV &

*/

#include <teem/nrrd.h>
#include <teem/ell.h>

#include <sgi_stl_warnings_off.h>
#include <string>
#include <iostream>
#include <sgi_stl_warnings_on.h>

extern "C" {
  void dsyev_(const char& jobz, const char& uplo,
	      const int& n, double data_array[],
	      const int& lda, double eigen_val[],
	      double dwork[], const int& ldwork,
	      int& info);
}

using namespace std;

void usage(char *me, const char *unknown = 0) {
  if (unknown) {
    fprintf(stderr, "%s: unknown argument %s\n", me, unknown);
  }
  // Print out the usage
  printf("-input  <filename>\n");
  printf("-output <filename>\n");
  printf("-numbases <int>\n");
  printf("-usegk\n");
  printf("-v - verbose\n");
  printf("-vv [int] - verbose level\n");
	 

  if (unknown)
    exit(1);
}

int main(int argc, char *argv[]) {
  char *me = argv[0];
  char *err;
  char *infilename = 0;
  char *outfilename_base = "trasimage";
  // -1 defaults to all
  int num_bases = 0;
  int num_channels;
  bool usegk = false;
  int verbose = 0;

  if (argc < 2) {
    usage(me);
    return 0;
  }

  for(int i = 1; i < argc; i++) {
    string arg(argv[i]);
    if (arg == "-input" || arg == "-i") {
      infilename = argv[++i];
    } else if (arg == "-output" || arg == "-o") {
      outfilename_base = argv[++i];
    } else if (arg == "-numbases" ) {
      num_bases = atoi(argv[++i]);
    } else if (arg == "-usegk" ) {
      usegk = true;
    } else if (arg == "-v" ) {
      verbose++;
    } else if (arg == "-vv" ) {
      verbose = atoi(argv[++i]);
    } else {
      usage(me, arg.c_str());
    }
  }

  // load input nrrd
  Nrrd *nin = nrrdNew();
  if (nrrdLoad(nin, infilename, 0)) {
    err = biffGet(NRRD);
    cerr << me << ": error loading NRRD: " << err << endl;
    free(err);
    biffDone(NRRD);
    exit(2);
  }

  // verify number of dimensions
  if (nin->dim != 3 ) {
    cerr << me << ":  number of dimesions " << nin->dim << " is not equal to 3." << endl;
    exit(2);
  }
  
  num_channels = nin->axis[0].size;

  if (usegk && num_channels != 3) {
    cerr << me << ":  size of axis[0] is not equal to 3" << endl;
    exit(2);
  }
  
  // Determine the number of eigen thingies to use
  if (num_bases < 1)
    // Use all of them
    num_bases = num_channels;
  // Check to make sure we haven't overstepped the max
  if (num_bases > num_channels) {
    cerr << "You have specified too many basis for the number of channels ("<<num_bases<<").  Clamping to "<< num_channels<<".\n";
    num_bases = num_channels;
  }

  // convert the type to floats if you need to
  if (nin->type != nrrdTypeFloat) {
    cerr << "Converting type from ";
    switch(nin->type) {
    case nrrdTypeUnknown: cerr << "nrrdTypeUnknown"; break;
    case nrrdTypeChar: cerr << "nrrdTypeChar"; break;
    case nrrdTypeUChar: cerr << "nrrdTypeUChar"; break;
    case nrrdTypeShort: cerr << "nrrdTypeShort"; break;
    case nrrdTypeUShort: cerr << "nrrdTypeUShort"; break;
    case nrrdTypeInt: cerr << "nrrdTypeInt"; break;
    case nrrdTypeUInt: cerr << "nrrdTypeUInt"; break;
    case nrrdTypeLLong: cerr << "nrrdTypeLLong"; break;
    case nrrdTypeULLong: cerr << "nrrdTypeULLong"; break;
    case nrrdTypeDouble: cerr << "nrrdTypeDouble"; break;
    default: cerr << "Unknown!!";
    }
    cerr << " to nrrdTypeFloat\n";
    Nrrd *new_n = nrrdNew();
    if (nrrdConvert(new_n, nin, nrrdTypeFloat)) {
      err = biffGet(NRRD);
      cerr << me << ": unable to convert nrrd: " << err << endl;
      biffDone(NRRD);
      exit(2);
    }
    // since the data was copied blow away the memory for the old nrrd
    nrrdNuke(nin);
    nin = new_n;
  }

  // compute the mean value
  Nrrd *meanY = nrrdNew();
  Nrrd *mean = nrrdNew();
  int E = 0;
  if (!E) E |= nrrdProject(meanY, nin, 2, nrrdMeasureMean, nrrdTypeDefault);
  if (!E) E |= nrrdProject(mean, meanY, 1, nrrdMeasureMean, nrrdTypeDefault);
  if (E) {
    err = biffGet(NRRD);
    cerr << me << ": computing the mean value failed: " << err << endl;
    biffDone(NRRD);
    exit(2);
  }

  nrrdAxisInfoSet(mean, nrrdAxisInfoLabel, "mean");

  // Free meanY
  meanY=nrrdNuke(meanY);
  
  // Write out the mean value
  string meanname(string(outfilename_base) + "-mean.nrrd");
  if (nrrdSave(meanname.c_str(), mean, 0)) {
    err = biffGet(NRRD);
    fprintf(stderr, "%s: trouble saving to %s: %s\n", me, meanname.c_str(), err);
    exit(2);
  }
  
  if (verbose) cout << "Wrote out mean to "<<meanname<<"\n";
  
  // Allocate our covariance matrix
  Nrrd *cov = nrrdNew();
  if (nrrdAlloc(cov, nrrdTypeDouble,2, num_channels, num_channels))
    {
      err = biffGetDone(NRRD);
      fprintf(stderr, "%s: error allocating covariance matrix:\n%s", me, err);
      exit(2);
    }
    
  // loop over each pixel to compute cov(x,y)
  int height = nin->axis[2].size;
  int width = nin->axis[1].size;
  float *data = (float*)nin->data;
  for (int y=0; y < height; y++) {
    for (int x = 0; x < width; x++ ) {
      // compute cov(x,y) for current pixel
      double *cov_data = (double*)(cov->data);
      for(int c = 0; c < num_channels; c++)
	for(int r = 0; r < num_channels; r++)
	  {
	    *cov_data = *cov_data + data[c] * data[r];
	    cov_data++;
	  }
      data += num_channels;
    }
  }

  if (verbose) cout << "Done computing first part of covariance matrix.\n";
  
  if (verbose > 10) {
    double *cov_data = (double*)(cov->data);
    for(int c = 0; c < cov->axis[1].size; c++)
      {
	cout << "[";
	for(int r = 0; r < cov->axis[0].size; r++)
	  {
	    cout << cov_data[c*cov->axis[0].size + r] << ", ";
	  }
	cout << "]\n";
      }
  }

  {
    double *cov_data = (double*)(cov->data);
    float *mean_data = (float*)(mean->data);
    float inv_num_pixels = 1.0f/(width*height);
    for(int c = 0; c < num_channels; c++)
      for(int r = 0; r < num_channels; r++)
	{
	  *cov_data = *cov_data * inv_num_pixels - mean_data[c] * mean_data[r];
	  cov_data++;
	}
  }

  if (verbose) cout << "After minus mean\n";

  if (verbose > 10)
  {
    double *cov_data = (double*)(cov->data);
    for(int r = 0; r < cov->axis[0].size; r++)
      {
	cout << "[";
	for(int c = 0; c < cov->axis[1].size; c++)
	  {
	    cout << cov_data[c*cov->axis[0].size + r] << ", ";
	  }
	cout << "]\n";
      }
  }
  
  // Covariance matrix computed

  // Compute eigen values/vectors

  double *eval = new double[num_channels];
  double *cov_data = (double*)(cov->data);
  double *evec_data = 0;

  if (verbose) cout << "Computing eigen stuffs\n";
  
  if (usegk) {
    // Here's where the general solution diverges to the RGB case.
    evec_data = new double[9];
    int roots = ell_3m_eigensolve_d(eval, evec_data, (double*)(cov->data), 1);
    if (roots != ell_cubic_root_three) {
      cerr << me << "Something with the eighen solve went haywire.  Did not get three roots but "<<roots<<" roots.\n";
      exit(2);
    }
  } else {
    const char jobz = 'V';
    const char uplo = 'L';
    const int N = num_channels;
    const int lda = N;
    int info;
    const int lwork = (3*N-1)*2;
    cout << "Wanting to allocate "<<lwork<<" doubles.\n";
    double *work = new double[lwork];
    if (!work) {
      cerr << "Could not allocate the memory for work\n";
      exit(2);
    }
    dsyev_(jobz, uplo, N, cov_data, lda, eval, work, lwork, info);
    delete[] work;
    if (info != 0) {
      cout << "Eigen solver did not converge.\n";
      cout << "info  = "<<info<<"\n";
      exit(2);
    }
    evec_data = cov_data;
  }

  if (verbose) cout << "Done computing eigen vectors and values.\n";
  
  if (verbose > 10) {
    cout << "Eigen values are [";
    for (int i = 0; i < num_channels; i++)
      cout << eval[i]<<", ";
    cout << "]\n";
  }
  delete[] eval;
  
  // Cull our eigen vectors
  Nrrd *transform = nrrdNew();
  if (nrrdAlloc(transform, nrrdTypeFloat, 2, num_channels, num_bases)) {
    err = biffGet(NRRD);
    fprintf(stderr, "%s: error allocating transform matrix :\n%s", me, err);
    exit(0);
  }

  nrrdAxisInfoSet(transform, nrrdAxisInfoLabel, "channels", "bases");
  
  {
    float *tdata = (float*)(transform->data);
    double *edata = evec_data;
    if (usegk) {
      for (int basis = 0; basis < num_bases; basis++) {
	for(int channel = 0; channel < num_channels; channel++) {
	  *tdata = edata[basis * num_channels + channel];
	  tdata++;
	}
      }
    } else {
      int basis_start = num_channels - 1;
      int basis_end = basis_start - num_bases;
      for (int basis = basis_start; basis > basis_end; basis--) {
	for(int channel = 0; channel < num_channels; channel++) {
	  *tdata = edata[basis * num_channels + channel];
	  tdata++;
	}
      }
    }

    // Free covariance matrix, eigenvectors
    cov = nrrdNuke(cov);
    cov_data = evec_data = 0;
    
    if (verbose > 10) {
      cout << "\ntransform matrix\n";
      tdata = (float*)(transform->data);
      for(int c = 0; c < num_bases; c++)
	{
	  cout << "[";
	  for(int r = 0; r < num_channels; r++)
	    {
	      cout << tdata[c*num_channels + r] << ", ";
	    }
	  cout << "]\n";
	}
      cout << "\n\n";
    }
  }

  if (verbose) cout << "Done filling transfer matrix.\n";
  
  // Compute our basis textures
  Nrrd *basesTextures = nrrdNew();
  if (nrrdAlloc(basesTextures, nrrdTypeFloat, 3, num_bases, width, height)) {
    err = biffGet(NRRD);
    fprintf(stderr, "%s: error allocating bases textures :\n%s", me, err);
    exit(0);
  }

  nrrdAxisInfoSet(basesTextures, nrrdAxisInfoLabel, "bases", "width", "height");
  
  {
    float *btdata = (float*)(basesTextures->data);
    float *data = (float*)(nin->data);
    for (int y=0; y < height; y++) {
      for (int x = 0; x < width; x++ ) {
	// Subtract the mean
	float *mdata = (float*)(mean->data);
	for(int i = 0; i < mean->axis[0].size; i++) {
	  data[i] -= mdata[i];
	}
	
	// Now do transform * data
	float *tdata = (float*)(transform->data);

	for(int c = 0; c < num_bases; c++)
	  for(int r = 0; r < num_channels; r++)
	    {
	      btdata[c] += tdata[c*num_channels+r]*data[r];
	    }
	btdata += num_bases;
	data += num_channels;
      }
    }
  }

  // Free input nrrd and mean
  nin = nrrdNuke(nin);
  mean = nrrdNuke(mean);
  
  // Write out the basis textures
  string basisname(string(outfilename_base) + "-bases.nrrd");
  if (nrrdSave(basisname.c_str(), basesTextures, 0)) {
    err = biffGet(NRRD);
    fprintf(stderr, "%s: trouble saving to %s: %s\n", me, basisname.c_str(), err);
    exit(2);
  }

  // Free bases textures
  basesTextures = nrrdNuke(basesTextures);

  // Write out the transformation matrix
  Nrrd *newTrans=nrrdNew();
  int axes[2] = {1,0};
  if (nrrdAxesPermute(newTrans, transform, axes)) {
    err = biffGetDone(NRRD);
    fprintf(stderr, "%s: error permuting the transformation matrix:\n%s", me, err);
    exit(2);
  }

  // Free old transform
  transform = nrrdNuke(transform);
  
  string transname(string(outfilename_base) + "-transform.nrrd");
  if (nrrdSave(transname.c_str(), newTrans, 0)) {
    err = biffGet(NRRD);
    fprintf(stderr, "%s: trouble saving to %s: %s\n", me, transname.c_str(), err);
    exit(2);
  }

  // Free new transform
  newTrans = nrrdNuke(newTrans);
    
  return 0;
}
