{
  "targets": [
    {
      "target_name": "pdfium",
      "sources": [
        "src/addon.cc",
        "src/document.cc",
        "src/render.cc",
        "src/objects.cc"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "vendor"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "conditions": [
        ["OS=='win'", {
          "libraries": [
            "<(module_root_dir)/vendor/pdfium.dll.lib"
          ],
          "copies": [{
            "files": ["<(module_root_dir)/vendor/pdfium.dll"],
            "destination": "<(PRODUCT_DIR)"
          }],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "AdditionalOptions": ["/std:c++17"]
            }
          }
        }],
        ["OS=='mac'", {
          "libraries": [
            "-L<(module_root_dir)/vendor",
            "-lpdfium"
          ],
          "copies": [{
            "files": ["<(module_root_dir)/vendor/libpdfium.dylib"],
            "destination": "<(PRODUCT_DIR)"
          }],
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
            "MACOSX_DEPLOYMENT_TARGET": "10.15",
            "OTHER_LDFLAGS": ["-Wl,-rpath,@loader_path"]
          }
        }],
        ["OS=='linux'", {
          "libraries": [
            "-L<(module_root_dir)/vendor",
            "-lpdfium"
          ],
          "copies": [{
            "files": ["<(module_root_dir)/vendor/libpdfium.so"],
            "destination": "<(PRODUCT_DIR)"
          }],
          "ldflags": ["-Wl,-rpath,'$$ORIGIN'"],
          "cflags_cc": ["-std=c++17", "-fexceptions"]
        }]
      ]
    }
  ]
}
