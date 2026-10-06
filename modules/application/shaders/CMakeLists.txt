rtsl_add_program("leaf-application-shaders" SOURCES 
	"${CMAKE_CURRENT_SOURCE_DIR}/application.rtsl"
)

rtsl_embed_program("leaf-application"
	SYMBOL leaf_application_shader
	PROGRAMS "leaf-application-shaders"
)
