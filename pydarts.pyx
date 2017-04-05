from libcpp.vector cimport vector


ctypedef char NODE_TYPE
ctypedef unsigned char NODE_U_TYPE
ctypedef int ARRAY_TYPE
ctypedef unsigned int ARRAY_U_TYPE
ctypedef NODE_TYPE KEY_TYPE
ctypedef ARRAY_TYPE VALUE_TYPE


cdef extern from "darts.cpp" namespace "Darts":

    struct RESULT_PAIR_TYPE:
        VALUE_TYPE value
        size_t length

    cdef cppclass DoubleArray:

        DoubleArray() except +
        void clear()
        size_t unit_size()
        size_t size()
        size_t total_size()
        size_t nonzero_size()
        int build(size_t,
                  const NODE_TYPE**,
                  const size_t*,
                  const VALUE_TYPE*,
                  int(*progress_func)(size_t, size_t)=0)
        int open(char*, char*, size_t, size_t)
        int save(const char*, const char*, size_t)
        RESULT_PAIR_TYPE exactMatchSearch(const KEY_TYPE*,
                                          size_t,
                                          size_t)
        size_t commonPrefixSearch(const KEY_TYPE*,
                                  RESULT_PAIR_TYPE*,
                                  size_t,
                                  size_t,
                                  size_t)
        VALUE_TYPE traverse(const KEY_TYPE *key,
                            size_t&,
                            size_t&,
                            size_t)


cdef class PyDoubleArray:

    cdef DoubleArray* c_da

    def __cinit__(self):
        self.c_da = new DoubleArray()

    def __dealloc__(self):
        del self.c_da

    def clear(self):
        return self.c_da.clear()

    def unit_size(self):
        return self.c_da.unit_size()

    def size(self):
        return self.c_da.size()

    def total_size(self):
        return self.c_da.total_size()

    def nonzero_size(self):
        return self.c_da.nonzero_size()

    def build(self, words, values=None):
        cdef vector[const NODE_TYPE*] c_keys
        cdef vector[VALUE_TYPE] c_values
        cdef const size_t* c_length = NULL
        cdef const VALUE_TYPE* c_zero_values = NULL

        for word in words:
            c_keys.push_back(word)
        if values is None:
            return self.c_da.build(c_keys.size(), &c_keys[0],
                                   c_length, c_zero_values)
        else:
            for value in values:
                c_values.push_back(value)
            return self.c_da.build(c_keys.size(), &c_keys[0],
                                   c_length, &c_values[0])

    def open(self, filepath, mode='rb', offset=0, size=0):
        cdef char* c_file = filepath
        cdef char* c_mode = mode
        cdef size_t c_offset = offset
        cdef size_t c_size = size
        return self.c_da.open(c_file, c_mode, c_offset, c_size)

    def save(self, filepath, mode='wb', offset=0):
        cdef const char* c_file = filepath
        cdef const char* c_mode = mode
        cdef size_t c_offset = offset
        return self.c_da.save(c_file, c_mode, c_offset)

    def exactMatchSearch(self, word, node_pos=0):
        cdef const KEY_TYPE* c_key = word
        cdef size_t c_length = len(word)
        cdef size_t c_node_pos = node_pos
        return self.c_da.exactMatchSearch(c_key, c_length, c_node_pos)

    def commonPrefixSearch(self, word, node_pos=0):
        cdef const KEY_TYPE* c_key = word
        cdef RESULT_PAIR_TYPE c_results[256];
        cdef size_t c_length = 0
        cdef size_t c_node_pos = node_pos
        num = self.c_da.commonPrefixSearch(c_key, &c_results[0], 256,
                                           c_length, c_node_pos)
        for i in range(num):
            yield c_results[i]

    def traverse(self, word, length=0):
        cdef const KEY_TYPE* c_key = word
        cdef size_t c_node_pos = 0
        cdef size_t c_key_pos = 0
        cdef size_t c_length = length
        ret = self.c_da.traverse(c_key, c_node_pos, c_key_pos, c_length)
        return {'node_pos': c_node_pos, 'key_pos': c_key_pos, 'ret': ret}
