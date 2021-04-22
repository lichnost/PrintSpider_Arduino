Import("env")

env.Append(LINKFLAGS=["-Wl,--undefined=_mmcu,--section-start=.mmcu=0x3800"])

def capture_callback(*args, **kwargs):
    try:
        import configparser
    except ImportError:
        import ConfigParser as configparser

    config = configparser.ConfigParser()
    config.read("platformio.ini")

    custom_arg = 'custom_capture'
    sections = config.sections()
    extra_args = []
    if config.has_option(sections[0], custom_arg):
        extra_args = config.get(sections[0], custom_arg).split()

    cmd_base = [
        "simavr",
        "$BUILD_DIR/${PROGNAME}.elf"]
    cmd_args = [
        "-m", env.get("BOARD_MCU"),
        "-f", env.get("BOARD_F_CPU"),
        "--output", "output.vcd"
    ]

    env.Execute(' '.join(map(str, cmd_base + cmd_args + extra_args)))

capture_alias = env.Alias("capture", "buildprog", capture_callback)
env.AlwaysBuild(capture_alias)
