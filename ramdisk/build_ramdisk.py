import subprocess
from pathlib import Path

RAMDISK_ROOT = Path("ramdisk_root")
RAMDISK_OUT_IMG = Path("ramdisk.img")
RAMDISK_SIZE = 20172800


def build_ramdisk_image(root_path: Path, output_img: Path, size: int):
    # Create a temporary dmg to hold the ramdisk contents
    tmp_dmg_path = output_img.with_suffix(".dmg")
    if tmp_dmg_path.exists():
        tmp_dmg_path.unlink()

    ramdisk_size_mb = (size + 1024 * 1024 - 1) // (1024 * 1024)
    subprocess.run(
        [
            "hdiutil",
            "create",
            "-size",
            f"{ramdisk_size_mb}mb",
            "-fs",
            "HFSX",
            "-volname",
            "Ramdisk",
            "-type",
            "UDIF",
            "-layout",
            "NONE",
            tmp_dmg_path.as_posix(),
        ],
        check=True,
    )

    # Mount the dmg and copy the contents of root_path to it, then unmount it
    mount_point = "/tmp/jb_ramdisk"
    subprocess.run(["hdiutil", "attach", tmp_dmg_path.as_posix(), "-mountpoint", mount_point], check=True)

    subprocess.run(["ditto", root_path.as_posix(), mount_point], check=True)
    subprocess.run(["hdiutil", "detach", mount_point], check=True)

    # Convert the dmg to a raw img
    output_img_cdr_path = output_img.with_suffix(".cdr")
    subprocess.run(
        ["hdiutil", "convert", tmp_dmg_path.as_posix(), "-format", "UDTO", "-o", output_img_cdr_path.as_posix()],
        check=True,
    )

    ramdisk_contents = output_img_cdr_path.read_bytes()
    output_img.write_bytes(ramdisk_contents[:size])

    tmp_dmg_path.unlink()
    output_img_cdr_path.unlink()


if __name__ == "__main__":
    ramdisk_root_path = RAMDISK_ROOT.resolve()
    ramdisk_out_img_path = RAMDISK_OUT_IMG.resolve()

    if not ramdisk_root_path.exists() or not ramdisk_root_path.is_dir():
        print(f"Error: {ramdisk_root_path} does not exist or is not a directory.")
        exit(1)

    if ramdisk_out_img_path.exists():
        ramdisk_out_img_path.unlink()

    build_ramdisk_image(ramdisk_root_path, ramdisk_out_img_path, RAMDISK_SIZE)
    print(f"Ramdisk built {ramdisk_out_img_path}")
