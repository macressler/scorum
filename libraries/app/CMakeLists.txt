file(GLOB HEADERS "include/scorum/app/*.hpp")
file(GLOB EGENESIS_HEADERS "../egenesis/include/scorum/app/*.hpp")

add_library( scorum_app
             database_api.cpp
             api.cpp
             application.cpp
             impacted.cpp
             plugin.cpp
             ${HEADERS}
             ${EGENESIS_HEADERS}
           )

target_link_libraries( scorum_app
                       scorum_chain
                       scorum_protocol
                       scorum_tags
                       scorum_follow
                       scorum_witness
                       scorum_mf_plugins
                       fc
                       graphene_net
                       graphene_utilities
                       ${PLATFORM_SPECIFIC_LIBS})
target_include_directories( scorum_app
                            PUBLIC
                            "${CMAKE_CURRENT_SOURCE_DIR}/include"
                            "${CMAKE_CURRENT_SOURCE_DIR}/../egenesis/include")

if(MSVC)
  set_source_files_properties( application.cpp api.cpp database_api.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

INSTALL( TARGETS
   scorum_app

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/scorum/app" )
