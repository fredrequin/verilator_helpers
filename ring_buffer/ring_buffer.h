// Copyright 2019-2023 Frederic Requin
//
// License: BSD
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions 
// are met:
//   - Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   - Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer 
//     in the documentation and/or other materials provided with the 
//     distribution.
//   - Neither the name of the author nor the names of its contributors 
//     may be used to endorse or promote products derived from this 
//     software without specific prior written permission.
//
// Ring buffer / FIFO C++ model:
// -----------------------------
//  - In theory, thread safe
//  - Mutex free (m_index access is atomic on 64-bit architectures)
//  - Buffer size is a power of 2

#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include "verilated.h"

template<typename T> class RingBuf
{
    public:
        // Constructor
        explicit RingBuf(int log2) :
            m_size  { 1U << (log2 & 31) },
            m_index { 0 }
        {
            m_array = new T[m_size];
        }
        // Destructor
        ~RingBuf()
        {
            m_size = 0;
            m_index.both = 0ULL;
            delete [] m_array;
        }
        // Flush FIFO
        inline void flush(void)
        {
            m_index.both = 0ULL;
        }
        // Is FIFO empty ?
        inline bool is_empty(void)
        {
            register index_t i;
            i.both = m_index.both;

            return (i.idx[1] == i.idx[0]);
        }
        // Is FIFO full ?
        inline bool is_full(void)
        {
            register index_t i;
            i.both = m_index.both;

            return ((i.idx[1] + m_size) == i.idx[0]);
        }
        // FIFO fullness
        inline vluint32_t level(void)
        {
            register index_t i;
            i.both = m_index.both;

            return (i.idx[1] - i.idx[0]);
        }
        // Write an element to the FIFO
        bool write(const T data)
        {
            register index_t i;
            i.both = m_index.both;
            
            if ((i.idx[1] + m_size) == i.idx[0])
            {
                return false;
            }
            else
            {
                m_array[i.idx[0] & (m_size - 1)] = data;
                m_index.idx[0] = i.idx[0] + 1;
                return true;
            }
        }
        // Read an element from the FIFO
        bool read(T &data)
        {
            register index_t i;
            i.both = m_index.both;
            
            data = m_array[i.idx[1] & (m_size - 1)];
            
            if (i.idx[1] == i.idx[0])
            {
                return false;
            }
            else
            {
                m_index.idx[1] = i.idx[1] + 1;
                return true;
            }
        }
    private:
        typedef union
        {
            vluint64_t both;
            vluint32_t idx[2]; // 0 : write index, 1 : read index
        } index_t;
        T               *m_array;
        const vluint32_t m_size;
        index_t          m_index;
};

#endif /* _RING_BUFFER_H_ */
