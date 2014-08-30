/* This file was generated by fulmar version 0.9.2. */

/*
 * Copyright (c) 2014 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef NEBO_RHS_H
   #define NEBO_RHS_H

   namespace SpatialOps {
      template<typename CurrentMode, typename AtomicType>
       struct NeboScalar;
      template<typename AtomicType>
       struct NeboScalar<Initial, AtomicType> {
         public:
          AtomicType typedef value_type;

          NeboScalar<SeqWalk, AtomicType> typedef SeqWalkType;

          #ifdef ENABLE_THREADS
             NeboScalar<Resize, AtomicType> typedef ResizeType;
          #endif
          /* ENABLE_THREADS */

          #ifdef __CUDACC__
             NeboScalar<GPUWalk, AtomicType> typedef GPUWalkType;
          #endif
          /* __CUDACC__ */

          NeboScalar(value_type const v)
          : value_(v)
          {}

          inline GhostData ghosts_with_bc(void) const {
             return GhostData(GHOST_MAX);
          }

          inline GhostData ghosts_without_bc(void) const {
             return GhostData(GHOST_MAX);
          }

          inline bool has_extents(void) const { return false; }

          inline IntVec extents(void) const { return IntVec(0, 0, 0); }

          inline IntVec has_bc(void) const { return IntVec(0, 0, 0); }

          inline SeqWalkType init(IntVec const & extents,
                                  GhostData const & ghosts,
                                  IntVec const & hasBC) const {
             return SeqWalkType(value_);
          }

          #ifdef ENABLE_THREADS
             inline ResizeType resize(void) const { return ResizeType(value_); }
          #endif
          /* ENABLE_THREADS */

          #ifdef __CUDACC__
             inline bool cpu_ready(void) const { return true; }

             inline bool gpu_ready(int const deviceIndex) const { return true; }

             inline GPUWalkType gpu_init(IntVec const & extents,
                                         GhostData const & ghosts,
                                         IntVec const & hasBC,
                                         int const deviceIndex) const {
                return GPUWalkType(value_);
             }

             #ifdef NEBO_GPU_TEST
                inline void gpu_prep(int const deviceIndex) const {}
             #endif
             /* NEBO_GPU_TEST */
          #endif
          /* __CUDACC__ */

         private:
          value_type const value_;
      };
      #ifdef ENABLE_THREADS
         template<typename AtomicType>
          struct NeboScalar<Resize, AtomicType> {
            public:
             AtomicType typedef value_type;

             NeboScalar<SeqWalk, AtomicType> typedef SeqWalkType;

             NeboScalar(value_type const value)
             : value_(value)
             {}

             inline SeqWalkType init(IntVec const & extents,
                                     GhostData const & ghosts,
                                     IntVec const & hasBC) const {
                return SeqWalkType(value_);
             }

            private:
             value_type const value_;
         }
      #endif
      /* ENABLE_THREADS */;
      template<typename AtomicType>
       struct NeboScalar<SeqWalk, AtomicType> {
         public:
          AtomicType typedef value_type;

          NeboScalar(value_type const value)
          : value_(value)
          {}

          inline value_type eval(int const x, int const y, int const z) const {
             return value_;
          }

         private:
          value_type const value_;
      };
      #ifdef __CUDACC__
         template<typename AtomicType>
          struct NeboScalar<GPUWalk, AtomicType> {
            public:
             AtomicType typedef value_type;

             NeboScalar(value_type const value)
             : value_(value)
             {}

             __device__ inline value_type eval(int const x,
                                               int const y,
                                               int const z) const {
                return value_;
             }

            private:
             value_type const value_;
         }
      #endif
      /* __CUDACC__ */;

      template<typename CurrentMode, typename FieldType>
       struct NeboConstField;
      template<typename FieldType>
       struct NeboConstField<Initial, FieldType> {
         public:
          FieldType typedef field_type;

          NeboConstField<SeqWalk, FieldType> typedef SeqWalkType;

          #ifdef ENABLE_THREADS
             NeboConstField<Resize, FieldType> typedef ResizeType;
          #endif
          /* ENABLE_THREADS */

          #ifdef __CUDACC__
             NeboConstField<GPUWalk, FieldType> typedef GPUWalkType;
          #endif
          /* __CUDACC__ */

          NeboConstField(FieldType const & f)
          : field_(f)
          {}

          inline GhostData ghosts_with_bc(void) const {
             return field_.get_valid_ghost_data() + point_to_ghost(field_.boundary_info().has_extra());
          }

          inline GhostData ghosts_without_bc(void) const {
             return field_.get_valid_ghost_data();
          }

          inline bool has_extents(void) const { return true; }

          inline IntVec extents(void) const {
             return field_.window_with_ghost().extent() - field_.get_valid_ghost_data().get_minus()
                    - field_.get_valid_ghost_data().get_plus();
          }

          inline IntVec has_bc(void) const {
             return field_.boundary_info().has_bc();
          }

          inline SeqWalkType init(IntVec const & extents,
                                  GhostData const & ghosts,
                                  IntVec const & hasBC) const {
             return SeqWalkType(field_);
          }

          #ifdef ENABLE_THREADS
             inline ResizeType resize(void) const { return ResizeType(field_); }
          #endif
          /* ENABLE_THREADS */

          #ifdef __CUDACC__
             inline bool cpu_ready(void) const {
                return field_.is_valid(CPU_INDEX);
             }

             inline bool gpu_ready(int const deviceIndex) const {
                return field_.is_valid(deviceIndex);
             }

             inline GPUWalkType gpu_init(IntVec const & extents,
                                         GhostData const & ghosts,
                                         IntVec const & hasBC,
                                         int const deviceIndex) const {
                return GPUWalkType(deviceIndex, field_);
             }

             #ifdef NEBO_GPU_TEST
                inline void gpu_prep(int const deviceIndex) const {
                   const_cast<FieldType *>(&field_)->add_device(deviceIndex);
                }
             #endif
             /* NEBO_GPU_TEST */
          #endif
          /* __CUDACC__ */

         private:
          FieldType const field_;
      };
      #ifdef ENABLE_THREADS
         template<typename FieldType>
          struct NeboConstField<Resize, FieldType> {
            public:
             FieldType typedef field_type;

             NeboConstField<SeqWalk, FieldType> typedef SeqWalkType;

             NeboConstField(FieldType const & f)
             : field_(f)
             {}

             inline SeqWalkType init(IntVec const & extents,
                                     GhostData const & ghosts,
                                     IntVec const & hasBC) const {
                return SeqWalkType(field_);
             }

            private:
             FieldType const field_;
         }
      #endif
      /* ENABLE_THREADS */;
      template<typename FieldType>
       struct NeboConstField<SeqWalk, FieldType> {
         public:
          FieldType typedef field_type;

          typename field_type::value_type typedef value_type;

          NeboConstField(FieldType const & f)
          : xGlob_(f.window_with_ghost().glob_dim(0)),
            yGlob_(f.window_with_ghost().glob_dim(1)),
            base_(f.field_values(CPU_INDEX) + (f.window_with_ghost().offset(0) +
                                               f.get_valid_ghost_data().get_minus(0))
                  + (f.window_with_ghost().glob_dim(0) * ((f.window_with_ghost().offset(1)
                                                           + f.get_valid_ghost_data().get_minus(1))
                                                          + (f.window_with_ghost().glob_dim(1)
                                                             * (f.window_with_ghost().offset(2)
                                                                + f.get_valid_ghost_data().get_minus(2))))))
          {}

          inline value_type eval(int const x, int const y, int const z) const {
             return base_[x + xGlob_ * (y + (yGlob_ * z))];
          }

         private:
          int const xGlob_;

          int const yGlob_;

          value_type const * base_;
      };
      #ifdef __CUDACC__
         template<typename FieldType>
          struct NeboConstField<GPUWalk, FieldType> {
            public:
             FieldType typedef field_type;

             typename field_type::value_type typedef value_type;

             NeboConstField(int const deviceIndex, FieldType const & f)
             : base_(f.field_values(deviceIndex) + (f.window_with_ghost().offset(0)
                                                    + f.get_valid_ghost_data().get_minus(0))
                     + (f.window_with_ghost().glob_dim(0) * ((f.window_with_ghost().offset(1)
                                                              + f.get_valid_ghost_data().get_minus(1))
                                                             + (f.window_with_ghost().glob_dim(1)
                                                                * (f.window_with_ghost().offset(2)
                                                                   + f.get_valid_ghost_data().get_minus(2)))))),
               xGlob_(f.window_with_ghost().glob_dim(0)),
               yGlob_(f.window_with_ghost().glob_dim(1))
             {}

             __device__ inline value_type eval(int const x,
                                               int const y,
                                               int const z) const {
                return base_[x + xGlob_ * (y + (yGlob_ * z))];
             }

            private:
             value_type const * base_;

             int const xGlob_;

             int const yGlob_;
         }
      #endif
      /* __CUDACC__ */;

      template<typename CurrentMode, typename T>
       struct NeboConstSingleValueField;
      template<typename T>
       struct NeboConstSingleValueField<Initial, T> {
         public:
          SpatialOps::SpatialField<SpatialOps::SingleValue, T> typedef
          field_type;

          SpatialOps::SpatialField<SpatialOps::SingleValue, T> typedef
          SingleValueFieldType;

          NeboConstSingleValueField<SeqWalk, T> typedef SeqWalkType;

          #ifdef ENABLE_THREADS
             NeboConstSingleValueField<Resize, T> typedef ResizeType;
          #endif
          /* ENABLE_THREADS */

          #ifdef __CUDACC__
             NeboConstSingleValueField<GPUWalk, T> typedef GPUWalkType;
          #endif
          /* __CUDACC__ */

          NeboConstSingleValueField(SingleValueFieldType const & f)
          : field_(f)
          {}

          inline GhostData ghosts_with_bc(void) const {
             return GhostData(GHOST_MAX);
          }

          inline GhostData ghosts_without_bc(void) const {
             return GhostData(GHOST_MAX);
          }

          inline bool has_extents(void) const { return false; }

          inline IntVec extents(void) const { return IntVec(0, 0, 0); }

          inline IntVec has_bc(void) const { return IntVec(0, 0, 0); }

          inline SeqWalkType init(IntVec const & extents,
                                  GhostData const & ghosts,
                                  IntVec const & hasBC) const {
             return SeqWalkType(* field_.field_values(CPU_INDEX));
          }

          #ifdef ENABLE_THREADS
             inline ResizeType resize(void) const {
                return ResizeType(* field_.field_values(CPU_INDEX));
             }
          #endif
          /* ENABLE_THREADS */

          #ifdef __CUDACC__
             inline bool cpu_ready(void) const {
                return field_.is_valid(CPU_INDEX);
             }

             inline bool gpu_ready(int const deviceIndex) const {
                return field_.is_valid(deviceIndex);
             }

             inline GPUWalkType gpu_init(IntVec const & extents,
                                         GhostData const & ghosts,
                                         IntVec const & hasBC,
                                         int const deviceIndex) const {
                return GPUWalkType(deviceIndex, field_);
             }

             #ifdef NEBO_GPU_TEST
                inline void gpu_prep(int const deviceIndex) const {
                   const_cast<SingleValueFieldType *>(&field_)->add_device(deviceIndex);
                }
             #endif
             /* NEBO_GPU_TEST */
          #endif
          /* __CUDACC__ */

         private:
          SingleValueFieldType const field_;
      };
      #ifdef ENABLE_THREADS
         template<typename T>
          struct NeboConstSingleValueField<Resize, T> {
            public:
             SpatialOps::SpatialField<SpatialOps::SingleValue, T> typedef
             field_type;

             NeboConstSingleValueField<SeqWalk, T> typedef SeqWalkType;

             NeboConstSingleValueField(double const & v)
             : value_(v)
             {}

             inline SeqWalkType init(IntVec const & extents,
                                     GhostData const & ghosts,
                                     IntVec const & hasBC) const {
                return SeqWalkType(value_);
             }

            private:
             double const value_;
         }
      #endif
      /* ENABLE_THREADS */;
      template<typename T>
       struct NeboConstSingleValueField<SeqWalk, T> {
         public:
          SpatialOps::SpatialField<SpatialOps::SingleValue, T> typedef
          field_type;

          typename field_type::value_type typedef value_type;

          NeboConstSingleValueField(double const & v)
          : value_(v)
          {}

          inline value_type eval(int const x, int const y, int const z) const {
             return value_;
          }

         private:
          double value_;
      };
      #ifdef __CUDACC__
         template<typename T>
          struct NeboConstSingleValueField<GPUWalk, T> {
            public:
             SpatialOps::SpatialField<SpatialOps::SingleValue, T> typedef
             field_type;

             typename field_type::value_type typedef value_type;

             SpatialOps::SpatialField<SpatialOps::SingleValue, T> typedef
             SingleValueFieldType;

             NeboConstSingleValueField(int const deviceIndex,
                                       SingleValueFieldType const & f)
             : pointer_(f.field_values(deviceIndex))
             {}

             __device__ inline value_type eval(int const x,
                                               int const y,
                                               int const z) const {
                return *pointer_;
             }

            private:
             value_type const * pointer_;
         }
      #endif
      /* __CUDACC__ */;
   } /* SpatialOps */

#endif
/* NEBO_RHS_H */
