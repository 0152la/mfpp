#!/usr/bin/env python3
import argparse
import datetime
import logging
import os
import random
import shlex
import shutil
import signal
import subprocess
import sys
import yaml

import pdb

###############################################################################
# Argument parsing
###############################################################################

parser = argparse.ArgumentParser(
    description = "Metalib batch experiment generator and runner")
parser.add_argument("config", type=str,
    help = "Path to configuration yaml file.")
parser.add_argument("--run-timeout", type=int, default=120,
    help = "Maximum time, in seconds, to allow execution of generated test cases.")
parser.add_argument("--test-count", type=int, default=100,
    help = "Number of tests to run; set `-1` for infinite tests.")
parser.add_argument("--debug", action='store_true',
    help = "If set, emit runtime debug information")
parser.add_argument("--append-id", action='store_true',
    help = "If set, appends a random numeric hash to the output folder")
parser.add_argument("--seed", type=int, default=random.randint(0, sys.maxsize),
    help = "Seed to initialize random generator in script.")
parser.add_argument("--always-log-out", action='store_true',
    help = "If set, always prints the output of STDOUT and STDERR for test"\
            " generation phases.")
parser.add_argument("--debug-to-file", action='store_true',
    help = "If set, emits debug output to log file.")

###############################################################################
# Helper functions
###############################################################################

def exec_cmd(name, cmd, test_id, timeout=None):
    if not timeout:
        log_console.debug(f"Running {name} command:\n\t*** {cmd}")
    else:
        log_console.debug(f"Running {name} command with t/o {timeout}:\n\t*** {cmd}")

    cmd_proc = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, encoding="utf-8")
    try:
        out, err = cmd_proc.communicate(timeout=timeout)
        proc_timeout = False
    except subprocess.TimeoutExpired:
        proc_timeout = True
        cmd_proc.kill()
        out, err = cmd_proc.communicate()
    log_runtime.info(f"{name} return code: {cmd_proc.returncode}")
    if cmd_proc.returncode != 0 or args.always_log_out:
        if proc_timeout:
            log_runtime.info(f"TIMEOUT {name} command")
        else:
            log_runtime.info(f"FAIL {name} command")
        log_runtime.debug(f"STDOUT:\n{out}")
        log_runtime.debug(f"STDERR:\n{err}")
    if proc_timeout:
        log_console.warning(f"Timeout {name} command for test count {test_id}!")
        return False
    if cmd_proc.returncode != 0:
        log_console.warning(f"Failed {name} command for test count {test_id}!")
        try:
            shutil.copyfile(full_output_file_name, f"{save_test_folder}/{name}_fail_{test_id:07d}")
        except FileNotFoundError:
            pass
    return cmd_proc.returncode == 0

def terminate_handler(sig, frame):
    log_console.info(f"Received terminate signal, finishing...")
    global terminate
    terminate = True

###############################################################################
# Main function
###############################################################################

if __name__ == '__main__':
    args = parser.parse_args()
    signal.signal(signal.SIGTERM, terminate_handler)

    if args.debug:
        log_level = logging.DEBUG
    else:
        log_level = logging.INFO

    log_console = logging.getLogger('console')
    log_console_handler = logging.StreamHandler(sys.stdout)
    log_console.addHandler(log_console_handler)
    log_console.setLevel(log_level)
    log_console.debug("Debug mode set")

    log_console.debug(f"Setting seed {args.seed}")
    random.seed(args.seed)

    log_console.debug(f"Parsing YAML config file {args.config}")
    with open(args.config, 'r') as config_fd:
        config = yaml.load(config_fd, Loader=yaml.FullLoader)

    log_console.debug(f"Setting cwd to {config['working_dir']}")
    os.chdir(config["working_dir"])
    output_folder = os.path.abspath(config['output_folder'])
    if args.append_id:
        output_folder += f"_{random.getrandbits(20):07d}"
    output_file_name = config['output_file_name']
    full_output_file_name = f"{output_folder}/{output_file_name}"
    if os.path.exists(output_folder):
        log_console.debug(f"Removing existing output folder {output_folder}.")
        shutil.rmtree(output_folder)
    log_console.debug(f"Creating output folder {output_folder}.")
    os.makedirs(output_folder, exist_ok=True)

    save_test_folder_name = "tests"
    save_test_folder = f"{output_folder}/{save_test_folder_name}"
    os.makedirs(save_test_folder, exist_ok=False)

    log_runtime_filename = "runtime.log"
    log_console.debug(f"Setting runtime log file `{log_runtime_filename}`")
    log_runtime = logging.getLogger('gentime')
    log_runtime.setLevel(log_level)
    log_runtime_handler = logging.FileHandler(f"{output_folder}/{log_runtime_filename}", 'w', "utf-8")
    log_runtime.addHandler(log_runtime_handler)
    if args.debug_to_file:
        log_console.addHandler(log_runtime_handler)

    test_count = 0
    terminate = False
    while test_count < args.test_count or args.test_count < 0:
        if terminate:
            break
        test_count += 1
        if not args.debug:
            log_console_handler.terminator = '\r'
        curr_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_console.debug(f"[{curr_time}] START test count {test_count} of {args.test_count}")

        gen_seed = random.randrange(sys.maxsize)
        log_console.debug(f"Generating test with seed {gen_seed}")
        log_runtime.info(f"===== Test count {test_count} with seed {gen_seed}")

        gen_cmd = f"./build/mtFuzzer {os.path.abspath(config['template_file'])}"\
              f" -o {output_folder}/{output_file_name}"\
              f" --lib-list={','.join([os.path.abspath(x) for x in config['lib_list']])}"\
              f" --set-meta-path {os.path.abspath(config['set_meta_path'])}"\
              f" --seed {gen_seed}"
        if not exec_cmd("generate", gen_cmd, test_count):
            continue

        compile_cmd = f"{os.path.abspath(config['compile_script'])} {output_folder}/{output_file_name}"
        old_cwd = os.getcwd()
        os.chdir(output_folder)
        if not exec_cmd("compile", compile_cmd, test_count):
            os.chdir(output_folder)
            continue
        os.chdir(old_cwd)

        run_output_file_name = os.path.splitext(f"{output_folder}/{output_file_name}")[0]
        run_cmd = f"{run_output_file_name}"
        if not exec_cmd("execute", run_cmd, test_count, timeout=args.run_timeout):
            continue


    log_console_handler.terminator = '\n'
    log_console.info(f"Finished experiments {output_folder}.")
