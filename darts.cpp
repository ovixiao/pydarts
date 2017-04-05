/*
  Darts -- Double-ARray Trie System

  $Id: darts.h.in 1674 2008-03-22 11:21:34Z taku $;

  Copyright(C) 2001-2007 Taku Kudo <taku@chasen.org>
*/
#ifndef DARTS_H_
#define DARTS_H_

#define DARTS_VERSION "0.32"
#include <vector>
#include <cstring>
#include <cstdio>


namespace Darts {
  typedef char NODE_TYPE;
  typedef unsigned char NODE_U_TYPE;

#if 4 == 2
    typedef short ARRAY_TYPE;
    typedef unsigned short ARRAY_U_TYPE;
#define DARTS_ARRAY_SIZE_IS_DEFINED 1
#endif

#if 4 == 4 && !defined(DARTS_ARRAY_SIZE_IS_DEFINED)
    typedef int ARRAY_TYPE;
    typedef unsigned int ARRAY_U_TYPE;
#define DARTS_ARRAY_SIZE_IS_DEFINED 1
#endif

#if 4 == 8 && !defined(DARTS_ARRAY_SIZE_IS_DEFINED)
    typedef long ARRAY_TYPE;
    typedef unsigned long ARRAY_U_TYPE;
#define DARTS_ARRAY_SIZE_IS_DEFINED 1
#endif

#if 4 == 8 && !defined(DARTS_ARRAY_SIZE_IS_DEFINED)
    typedef long long ARRAY_TYPE;
    typedef unsigned long long ARRAY_U_TYPE;
#define DARTS_ARRAY_SIZE_IS_DEFINED 1
#endif

  typedef ARRAY_TYPE VALUE_TYPE;
  typedef NODE_TYPE KEY_TYPE;
  typedef ARRAY_TYPE RESULT_TYPE;

  struct RESULT_PAIR_TYPE {
    VALUE_TYPE value;
    size_t length;
  };

  template <class T> inline T _max(T x, T y) { return(x > y) ? x : y; }
  template <class T> inline T* _resize(T* ptr, size_t n, size_t l, T v) {
    T *tmp = new T[l];
    for (size_t i = 0; i < n; ++i) tmp[i] = ptr[i];
    for (size_t i = n; i < l; ++i) tmp[i] = v;
    delete [] ptr;
    return tmp;
  }

  class DoubleArray {
  public:

    DoubleArray(): array_(0), used_(0), size_(0), alloc_size_(0), no_delete_(0), error_(0) {};
    virtual ~DoubleArray() { clear(); }

    size_t get_length(const NODE_TYPE *key) const {
      size_t i;
      for (i = 0; key[i] != static_cast<NODE_TYPE>(0); ++i) {};
      return i;
    };

    void set_result(VALUE_TYPE *x, VALUE_TYPE r, size_t) const {
      *x = r;
    }

    void set_result(RESULT_PAIR_TYPE *x, VALUE_TYPE r, size_t l) const {
      x->value = r;
      x->length = l;
    }

    void set_array(void *ptr, size_t size = 0) {
      clear();
      array_ = reinterpret_cast<unit_t *>(ptr);
      no_delete_ = true;
      size_ = size;
    }

    const void *array() const {
      return const_cast<const void *>(reinterpret_cast<void *>(array_));
    }

    void clear() {
      if (!no_delete_)
        delete [] array_;
      delete [] used_;
      array_ = 0;
      used_ = 0;
      alloc_size_ = 0;
      size_ = 0;
      no_delete_ = false;
    }

    size_t unit_size()  const { return sizeof(unit_t); }
    size_t size()       const { return size_; }
    size_t total_size() const { return size_ * sizeof(unit_t); }

    size_t nonzero_size() const {
      size_t result = 0;
      for (size_t i = 0; i < size_; ++i)
        if (array_[i].check) ++result;
      return result;
    }

    int build(size_t     key_size,
              const KEY_TYPE   **key,
              const size_t     *length = 0,
              const VALUE_TYPE *value = 0,
              int (*progress_func)(size_t, size_t) = 0) {
      if (!key_size || !key) return 0;

      progress_func_ = progress_func;
      key_           = key;
      length_        = length;
      key_size_      = key_size;
      value_         = value;
      progress_      = 0;

      resize(8192);

      array_[0].base = 1;
      next_check_pos_ = 0;

      node_t root_node;
      root_node.left  = 0;
      root_node.right = key_size;
      root_node.depth = 0;

      std::vector<node_t> siblings;
      fetch(root_node, siblings);
      insert(siblings);

      size_ += (1 << 8 * sizeof(KEY_TYPE)) + 1;
      if (size_ >= alloc_size_) resize(size_);

      delete [] used_;
      used_ = 0;

      return error_;
    }

    int open(const char *file,
             const char *mode = "rb",
             size_t offset = 0,
             size_t size = 0) {
      std::FILE *fp = std::fopen(file, mode);
      if (!fp) return -1;
      if (std::fseek(fp, offset, SEEK_SET) != 0) return -1;

      if (!size) {
        if (std::fseek(fp, 0L,     SEEK_END) != 0) return -1;
        size = std::ftell(fp);
        if (std::fseek(fp, offset, SEEK_SET) != 0) return -1;
      }

      clear();

      size_ = size;
      size_ /= sizeof(unit_t);
      array_ = new unit_t[size_];
      if (size_ != std::fread(reinterpret_cast<unit_t *>(array_),
                              sizeof(unit_t), size_, fp)) return -1;
      std::fclose(fp);

      return 0;
    }

    int save(const char *file,
             const char *mode = "wb",
             size_t offset = 0) {
      if (!size_) return -1;
      std::FILE *fp = std::fopen(file, mode);
      if (!fp) return -1;
      if (size_ != std::fwrite(reinterpret_cast<unit_t *>(array_),
                               sizeof(unit_t), size_, fp))
        return -1;
      std::fclose(fp);
      return 0;
    }

    inline void exactMatchSearch(const KEY_TYPE *key,
                                 RESULT_PAIR_TYPE & result,
                                 size_t len = 0,
                                 size_t node_pos = 0) const {
      result = exactMatchSearch(key, len, node_pos);
      return;
    }

    inline RESULT_PAIR_TYPE exactMatchSearch(const KEY_TYPE *key,
                                             size_t len = 0,
                                             size_t node_pos = 0) const {
      if (!len) len = get_length(key);

      RESULT_PAIR_TYPE result;
      set_result(&result, -1, 0);

      register ARRAY_TYPE  b = array_[node_pos].base;
      register ARRAY_U_TYPE p;

      for (register size_t i = 0; i < len; ++i) {
        p = b +(NODE_U_TYPE)(key[i]) + 1;
        if (static_cast<ARRAY_U_TYPE>(b) == array_[p].check)
          b = array_[p].base;
        else
          return result;
      }

      p = b;
      ARRAY_TYPE n = array_[p].base;
      if (static_cast<ARRAY_U_TYPE>(b) == array_[p].check && n < 0)
        set_result(&result, -n-1, len);

      return result;
    }

    size_t commonPrefixSearch(const KEY_TYPE *key,
                              RESULT_PAIR_TYPE* result,
                              size_t result_len,
                              size_t len = 0,
                              size_t node_pos = 0) const {
      if (!len) len = get_length(key);

      register ARRAY_TYPE  b   = array_[node_pos].base;
      register size_t     num = 0;
      register ARRAY_TYPE  n;
      register ARRAY_U_TYPE p;

      for (register size_t i = 0; i < len; ++i) {
        p = b;  // + 0;
        n = array_[p].base;
        if ((ARRAY_U_TYPE) b == array_[p].check && n < 0) {
          // result[num] = -n-1;
          if (num < result_len) set_result(&result[num], -n-1, i);
          ++num;
        }

        p = b +(NODE_U_TYPE)(key[i]) + 1;
        if ((ARRAY_U_TYPE) b == array_[p].check)
          b = array_[p].base;
        else
          return num;
      }

      p = b;
      n = array_[p].base;

      if ((ARRAY_U_TYPE)b == array_[p].check && n < 0) {
        if (num < result_len) set_result(&result[num], -n-1, len);
        ++num;
      }

      return num;
    }

    VALUE_TYPE traverse(const KEY_TYPE *key,
                        size_t &node_pos,
                        size_t &key_pos,
                        size_t len = 0) const {
      if (!len) len = get_length(key);

      register ARRAY_TYPE  b = array_[node_pos].base;
      register ARRAY_U_TYPE p;

      for (; key_pos < len; ++key_pos) {
        p = b + (NODE_U_TYPE)(key[key_pos]) + 1;
        if (static_cast<ARRAY_U_TYPE>(b) == array_[p].check) {
          node_pos = p;
          b = array_[p].base;
        } else {
          return -2;  // no node
        }
      }

      p = b;
      ARRAY_TYPE n = array_[p].base;
      if (static_cast<ARRAY_U_TYPE>(b) == array_[p].check && n < 0)
        return -n-1;

      return -1;  // found, but no value
    }

  private:
    struct node_t {
      ARRAY_U_TYPE code;
      size_t     depth;
      size_t     left;
      size_t     right;
    };

    struct unit_t {
      ARRAY_TYPE   base;
      ARRAY_U_TYPE check;
    };

    unit_t        *array_;
    unsigned char *used_;
    size_t        size_;
    size_t        alloc_size_;
    size_t        key_size_;
    const NODE_TYPE    **key_;
    const size_t        *length_;
    const ARRAY_TYPE   *value_;
    size_t        progress_;
    size_t        next_check_pos_;
    bool          no_delete_;
    int           error_;
    int (*progress_func_)(size_t, size_t);

    size_t resize(const size_t new_size) {
      unit_t tmp;
      tmp.base = 0;
      tmp.check = 0;
      array_ = _resize(array_, alloc_size_, new_size, tmp);
      used_  = _resize(used_, alloc_size_, new_size,
                       static_cast<unsigned char>(0));
      alloc_size_ = new_size;
      return new_size;
    }

    size_t fetch(const node_t &parent, std::vector <node_t> &siblings) {
      if (error_ < 0) return 0;

      ARRAY_U_TYPE prev = 0;

      for (size_t i = parent.left; i < parent.right; ++i) {
        if ((length_ ? length_[i] : get_length(key_[i])) < parent.depth)
          continue;

        const NODE_U_TYPE *tmp = reinterpret_cast<const NODE_U_TYPE *>(key_[i]);

        ARRAY_U_TYPE cur = 0;
        if ((length_ ? length_[i] : get_length(key_[i])) != parent.depth)
          cur = (ARRAY_U_TYPE)tmp[parent.depth] + 1;

        if (prev > cur) {
          error_ = -3;
          return 0;
        }

        if (cur != prev || siblings.empty()) {
          node_t tmp_node;
          tmp_node.depth = parent.depth + 1;
          tmp_node.code  = cur;
          tmp_node.left  = i;
          if (!siblings.empty()) siblings[siblings.size()-1].right = i;

          siblings.push_back(tmp_node);
        }

        prev = cur;
      }

      if (!siblings.empty())
        siblings[siblings.size()-1].right = parent.right;

      return siblings.size();
    }

    size_t insert(const std::vector <node_t> &siblings) {
      if (error_ < 0) return 0;

      size_t begin = 0;
      size_t pos   = _max((size_t)siblings[0].code + 1, next_check_pos_) - 1;
      size_t nonzero_num = 0;
      int    first = 0;

      if (alloc_size_ <= pos) resize(pos + 1);

      while (true) {
      next:
        ++pos;

        if (alloc_size_ <= pos) resize(pos + 1);

        if (array_[pos].check) {
          ++nonzero_num;
          continue;
        } else if (!first) {
          next_check_pos_ = pos;
          first = 1;
        }

        begin = pos - siblings[0].code;
        if (alloc_size_ <= (begin + siblings[siblings.size()-1].code))
          resize(static_cast<size_t>(alloc_size_ *
                                     _max(1.05, 1.0 * key_size_ / progress_)));

        if (used_[begin]) continue;

        for (size_t i = 1; i < siblings.size(); ++i)
          if (array_[begin + siblings[i].code].check != 0) goto next;

        break;
      }

      // -- Simple heuristics --
      // if the percentage of non-empty contents in check between the index
      // 'next_check_pos' and 'check' is greater than some constant
      // value(e.g. 0.9),
      // new 'next_check_pos' index is written by 'check'.
      if (1.0 * nonzero_num/(pos - next_check_pos_ + 1) >= 0.95)
        next_check_pos_ = pos;

      used_[begin] = 1;
      size_ = _max(size_,
                   begin +
                   static_cast<size_t>(siblings[siblings.size() - 1].code + 1));

      for (size_t i = 0; i < siblings.size(); ++i)
        array_[begin + siblings[i].code].check = begin;

      for (size_t i = 0; i < siblings.size(); ++i) {
        std::vector <node_t> new_siblings;

        if (!fetch(siblings[i], new_siblings)) {
          array_[begin + siblings[i].code].base =
            value_ ?
            static_cast<ARRAY_TYPE>(-value_[siblings[i].left]-1) :
            static_cast<ARRAY_TYPE>(-siblings[i].left-1);

          if (value_ && (ARRAY_TYPE)(-value_[siblings[i].left]-1) >= 0) {
            error_ = -2;
            return 0;
          }

          ++progress_;
          if (progress_func_)(*progress_func_)(progress_, key_size_);

        } else {
          size_t h = insert(new_siblings);
          array_[begin + siblings[i].code].base = h;
        }
      }

      return begin;
    }

  };  // class
}
#endif
