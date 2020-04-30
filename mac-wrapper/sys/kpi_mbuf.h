#ifndef WRAPPER_KPI_MBUF_H
#define WRAPPER_KPI_MBUF_H

#include <cstring>
#include <vector>
#include <cstdint>

struct mbuf_s
{
	std::vector<uint8_t> v;
	mbuf_s *next;
};

typedef struct mbuf_s* mbuf_t;

inline size_t mbuf_len(mbuf_t mb) { return mb->v.size(); }
inline size_t mbuf_maxlen(mbuf_t mb) { return mb->v.size(); } // TODO: implement me correctly
inline void mbuf_setlen(mbuf_t mb, size_t s) { mb->v.resize(s); }
inline mbuf_t mbuf_next(mbuf_t mb) { return mb->next; } // next might not be necessary if generating always single instance.
inline void *mbuf_data(mbuf_t mb) { return &mb->v[0]; }

#endif // WRAPPER_KPI_MBUF_H
