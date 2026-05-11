        #include <cmrc/cmrc.hpp>
#include <map>
#include <utility>

namespace cmrc {
namespace res {

namespace res_chars {
// These are the files which are available in this resource library
// Pointers to src/images/trophy.wav
extern const char* const f_7b4f_src_images_trophy_wav_begin;
extern const char* const f_7b4f_src_images_trophy_wav_end;
// Pointers to src/images/bronze.png
extern const char* const f_b2ed_src_images_bronze_png_begin;
extern const char* const f_b2ed_src_images_bronze_png_end;
// Pointers to src/images/gold.png
extern const char* const f_967c_src_images_gold_png_begin;
extern const char* const f_967c_src_images_gold_png_end;
// Pointers to src/images/platinum.png
extern const char* const f_d9ae_src_images_platinum_png_begin;
extern const char* const f_d9ae_src_images_platinum_png_end;
// Pointers to src/images/silver.png
extern const char* const f_1275_src_images_silver_png_begin;
extern const char* const f_1275_src_images_silver_png_end;
}

namespace {

const cmrc::detail::index_type&
get_root_index() {
    static cmrc::detail::directory root_directory_;
    static cmrc::detail::file_or_directory root_directory_fod{root_directory_};
    static cmrc::detail::index_type root_index;
    root_index.emplace("", &root_directory_fod);
    struct dir_inl {
        class cmrc::detail::directory& directory;
    };
    dir_inl root_directory_dir{root_directory_};
    (void)root_directory_dir;
    static auto f_25d9_src_dir = root_directory_dir.directory.add_subdir("src");
    root_index.emplace("src", &f_25d9_src_dir.index_entry);
    static auto f_63d5_src_images_dir = f_25d9_src_dir.directory.add_subdir("images");
    root_index.emplace("src/images", &f_63d5_src_images_dir.index_entry);
    root_index.emplace(
        "src/images/trophy.wav",
        f_63d5_src_images_dir.directory.add_file(
            "trophy.wav",
            res_chars::f_7b4f_src_images_trophy_wav_begin,
            res_chars::f_7b4f_src_images_trophy_wav_end
        )
    );
    root_index.emplace(
        "src/images/bronze.png",
        f_63d5_src_images_dir.directory.add_file(
            "bronze.png",
            res_chars::f_b2ed_src_images_bronze_png_begin,
            res_chars::f_b2ed_src_images_bronze_png_end
        )
    );
    root_index.emplace(
        "src/images/gold.png",
        f_63d5_src_images_dir.directory.add_file(
            "gold.png",
            res_chars::f_967c_src_images_gold_png_begin,
            res_chars::f_967c_src_images_gold_png_end
        )
    );
    root_index.emplace(
        "src/images/platinum.png",
        f_63d5_src_images_dir.directory.add_file(
            "platinum.png",
            res_chars::f_d9ae_src_images_platinum_png_begin,
            res_chars::f_d9ae_src_images_platinum_png_end
        )
    );
    root_index.emplace(
        "src/images/silver.png",
        f_63d5_src_images_dir.directory.add_file(
            "silver.png",
            res_chars::f_1275_src_images_silver_png_begin,
            res_chars::f_1275_src_images_silver_png_end
        )
    );
    return root_index;
}

}

cmrc::embedded_filesystem get_filesystem() {
    static auto& index = get_root_index();
    return cmrc::embedded_filesystem{index};
}

} // res
} // cmrc
    