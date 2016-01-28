if(APPLE)

set_target_properties(
  ${APPNAME}
  PROPERTIES
    MACOSX_BUNDLE_INFO_STRING "i-score, an interactive sequencer for the intermedia arts"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.i-score"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${ISCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_NAME "i-score"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${ISCORE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${ISCORE_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "The i-score team"
    MACOSX_BUNDLE_ICON_FILE "i-score.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

# Copy our dylibs if necessary
if(NOT ISCORE_STATIC_PLUGINS)
    set(ISCORE_BUNDLE_PLUGINS_FOLDER "${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/")
    function(iscore_copy_osx_plugin theTarget)
        add_custom_command(
          TARGET ${APPNAME} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${theTarget}> ${ISCORE_BUNDLE_PLUGINS_FOLDER})
    if(TARGET ${APPNAME}_unity)
      add_custom_command(
      TARGET ${APPNAME}_unity POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${theTarget}_unity> ${ISCORE_BUNDLE_PLUGINS_FOLDER})
    endif()
    endfunction()

    # Copy iscore plugins into the app bundle
    add_custom_command(TARGET ${APPNAME} POST_BUILD
                       COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/)
  if(TARGET ${APPNAME}_unity)
    add_custom_command(TARGET ${APPNAME}_unity POST_BUILD
               COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/)
  endif()

    foreach(plugin ${ISCORE_PLUGINS_LIST})
      iscore_copy_osx_plugin(${plugin})
    endforeach()
endif()

# Jamoma needs its own handling since he wants to be in Frameworks/jamoma
find_package(Jamoma REQUIRED)
# Copy the Jamoma files in the bundle generated by the build phase
copy_in_bundle_jamoma(${APPNAME} ${CMAKE_INSTALL_PREFIX}/${APPNAME}.app "${JAMOMA_LIBS}" "${JAMOMA_PLUGINS}")

if(TARGET ${APPNAME}_unity)
    copy_in_bundle_jamoma(${APPNAME}_unity ${CMAKE_INSTALL_PREFIX}/${APPNAME}.app "${JAMOMA_LIBS}" "${JAMOMA_PLUGINS}")
endif()
get_target_property(JAMOMA_LIBRARY_DIR Jamoma::Foundation LOCATION)
get_filename_component(JAMOMA_LIBRARY_DIR ${JAMOMA_LIBRARY_DIR} PATH)

# set-up Qt stuff.
# Remember to set CMAKE_INSTALL_PREFIX on the CMake command line.
get_target_property(QT_LIBRARY_DIR Qt5::Core LOCATION)
get_filename_component(QT_LIBRARY_DIR ${QT_LIBRARY_DIR} PATH)
get_filename_component(QT_LIBRARY_DIR "${QT_LIBRARY_DIR}/.." ABSOLUTE)
set(QT_PLUGINS_DIR "${Qt5Widgets_DIR}/../../../plugins")

set(plugin_dest_dir "${APPNAME}.app/Contents/PlugIns")
set(qtconf_dest_dir "${APPNAME}.app/Contents/Resources")

install(FILES "${QT_PLUGINS_DIR}/platforms/libqcocoa.dylib" DESTINATION "${plugin_dest_dir}/platforms")
install(FILES "${QT_PLUGINS_DIR}/imageformats/libqsvg.dylib" DESTINATION "${plugin_dest_dir}/imagesformats")
install(FILES "${QT_PLUGINS_DIR}/iconengines/libqsvgicon.dylib" DESTINATION "${plugin_dest_dir}/iconengines")

install(CODE "
    file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}/qt.conf\" \"[Paths]
Plugins = PlugIns
\")
" )
#Translations = Resources/translations
#Data = Resources

if(ISCORE_STATIC_PLUGINS)
    install(CODE "
        file(GLOB_RECURSE QTPLUGINS
            \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/*.dylib\")
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        fixup_bundle(
          \"\${CMAKE_INSTALL_PREFIX}/${APPNAME}.app\"
          \"\${QTPLUGINS}\"
          \"${QT_LIBRARY_DIR};${JAMOMA_LIBRARY_DIR}\")
        " COMPONENT Runtime)
else()
    set(CMAKE_INSTALL_RPATH "plugins")
    foreach(plugin ${ISCORE_PLUGINS_LIST})
        list(APPEND ISCORE_BUNDLE_INSTALLED_PLUGINS "${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/lib${plugin}.dylib")
    endforeach()

    install(CODE "
      message(${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins)
        file(GLOB_RECURSE QTPLUGINS
            \"\${CMAKE_INSTALL_PREFIX}/${plugin_dest_dir}/*.dylib\")
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        fixup_bundle(
           \"${CMAKE_INSTALL_PREFIX}/i-score.app\"
           \"\${QTPLUGINS};${ISCORE_BUNDLE_INSTALLED_PLUGINS}\"
       \"${QT_LIBRARY_DIR};${JAMOMA_LIBRARY_DIR};${CMAKE_BINARY_DIR}/plugins;${CMAKE_INSTALL_PREFIX}/plugins;${CMAKE_BINARY_DIR}/API/Implementations/Jamoma;${CMAKE_BINARY_DIR}/base/lib;${CMAKE_INSTALL_PREFIX}/${APPNAME}.app/Contents/MacOS/plugins/\"
        )
message(\"${ISCORE_ROOT_SOURCE_DIR}/CMake/Deployment/OSX/set_rpath.sh\"
          \"${CMAKE_INSTALL_PREFIX}/i-score.app/Contents/MacOS/plugins\")
execute_process(COMMAND
          \"${ISCORE_ROOT_SOURCE_DIR}/CMake/Deployment/OSX/set_rpath.sh\"
          \"${CMAKE_INSTALL_PREFIX}/i-score.app/Contents/MacOS/plugins\")
      ")
endif()

# After installation and fix-up by DeployQt5, we import the Jamoma libraries.
# They are put in the RPATH.
fixup_bundle_jamoma(${CMAKE_INSTALL_PREFIX}/${APPNAME}.app ${APPNAME} "${JAMOMA_LIBS}")

set(CPACK_GENERATOR "DragNDrop")

endif()
