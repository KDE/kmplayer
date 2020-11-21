#add_subdirectory(gst)

add_subdirectory(phonon)

if (KMPLAYER_WITH_NPP)
add_subdirectory(npp)
endif()

#add_subdirectory(xine)
#add_subdirectory(xvp)
