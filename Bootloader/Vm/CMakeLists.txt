include_directories(.)
ADD_LIBRARY(vmmgr ./mapList.c ./vmMgr.c)
ADD_LIBRARY(tlsf ./tlsf/tlsf.c)
ADD_LIBRARY(minilzo ./minilzo.c)
ADD_LIBRARY(vmmgr_mmu ./mmu.c)



