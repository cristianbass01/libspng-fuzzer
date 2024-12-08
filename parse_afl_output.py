import sys
import os
import subprocess
import shutil
import re

class CrashLog:
    def __init__(self):
        self.crash_log = {}

    def add_crash(self, crash_type, filename, line_number, column_number, function_name):
        key = (filename, line_number, column_number,function_name)
        if crash_type not in self.crash_log:
            print("Adding new crash type: {}".format(crash_type))
            self.crash_log[crash_type] = {}
        if key not in self.crash_log[crash_type]:
            self.crash_log[crash_type][key] = 1
        else:
            self.crash_log[crash_type][key] += 1

    def print_crash_log(self):
        if self.crash_log == {}:
            print("No crashes detected.")
            return
        for crash_type in self.crash_log:
            print("Crash type: {}".format(crash_type))
            for key, counter in self.crash_log[crash_type].items():
                filename = key[0]
                line_number = key[1]
                column_number = key[2]
                function_name = key[3]
                print("File: {}, \nLine: {}, \nColumn: {}, \nFunction: {}, \nCount: {}".format(filename, line_number, column_number, function_name, counter))
                print("-----\n")
            print("--------------------\n\n")



def parse_src_from_file_name(file_name):
    try:
        regex = re.compile(r"src:([0-9]+)")
        match = regex.search(file_name)
        return int(match.group(1))
    
    except Exception as e:
        print("Error: could not parse file name. Format incorrect.")
        print(e)
        sys.exit(1)

def parse_id_from_file_name(file_name):
    try:
        regex = re.compile(r"id:([0-9]+)")
        match = regex.search(file_name)
        return int(match.group(1))
    
    except Exception as e:
        print("Error: could not parse file name. Format incorrect.")
        print(e)
        sys.exit(1)


def error_detector(output, crash_log):

    regex_type = re.compile(r"MemorySanitizer:\s+([a-zA-Z0-9_-]+)")
    match_type = regex_type.search(output)
    if not match_type:
        print("Error: could not parse error type.")

    regex_line = re.compile(r"/([a-zA-Z0-9_/-]+\.c):([0-9]+):([0-9]+)")
    match_line = regex_line.search(output)
    if not match_line:
        print("Error: could not parse file name, line number and column number.")
    
    regex_function = re.compile(r"0x[0-9a-f]+ in ([a-zA-Z0-9_-]+)")
    match_function = regex_function.search(output)

    if not match_function:
        print("Error: could not parse function name.")
        sys.exit(1)
    if match_type and match_line and match_function:
        error_type = match_type.group(1)
        print("Error type: {}".format(error_type))
        filename = match_line.group(1)
        line_number = match_line.group(2)
        print("File: {}, Line: {}".format(filename, line_number))
        column_number = match_line.group(3)
        function_name = match_function.group(1)
        print("Function: {}".format(function_name))
        crash_log.add_crash(error_type, filename, line_number, column_number, function_name)
        return error_type, function_name
    else:
        print("Error: could not parse crash log. It doesn't look like MSan output.")


if __name__ == "__main__":

    if len(sys.argv) < 3:
        print("Usage: {} <afl_output_dir> <version>".format(sys.argv[0]))
        sys.exit(1)

    afl_output_dir = sys.argv[1]
    afl_version = sys.argv[2]
    afl_crash_dir = "{}/default/crashes".format(afl_output_dir)
    afl_queue_dir = "{}/default/queue".format(afl_output_dir)
    if not os.path.isdir(afl_crash_dir):
        print("Error: crash folder missing in output directory")
        sys.exit(1)

    if not os.path.isdir(afl_queue_dir):
        print("Error: queue folder missing in output directory")
        sys.exit(1)

    if not os.path.exists("fuzz/{}".format(afl_version)):
        print("Error: fuzz/{} does not exist".format(sys.argv[2]))
        sys.exit(1)
    
    if not os.path.exists("analyzed_crashes/"):
        os.mkdir("analyzed_crashes/")

    if not os.path.exists("unique_images/"):
        print("Error: input directory unique_images/ does not exist")
        sys.exit(1)

    # Create database of images id to be able to find the source image of a crash.
    img_db = {i: name for i, name in enumerate(os.listdir("unique_images/"))}
    crash_log = CrashLog()

    original_imgs = os.listdir(afl_queue_dir)

    original_img_id_db = {}
    for img in original_imgs:
        id_regex = re.compile(r"id:([0-9]+)")
        match = id_regex.search(img)
        if match:
            img_id = int(match.group(1))
            original_img_id_db[img_id] = img 
    
    original_img_orig_db = {}

    for img_id, img_name in original_img_id_db.items():
        for img_id, img_name in original_img_id_db.items():
            orig_regex = re.compile(r"orig:([0-9A-Za-z]+\.png)")
            match = orig_regex.search(img_name)
            if match:
                original_img_orig_db[img_id] = match.group(1)
            else:
                original_img_orig_db[img_id] = parse_src_from_file_name(img_name)

    for level in range(1, 20):
        for img_id, src in original_img_orig_db.items():
            if type(src) == int:
                try:
                    original_img_orig_db[img_id] = original_img_orig_db[src]
                except KeyError:         
                    pass
    
    # Remove images whose original version couldnt be determined.
    original_img_orig_db = {key:value for key, value in original_img_orig_db.items() if type(value) == str}
    print("Size of original image database: {}".format(len(original_img_orig_db)))
    count_processed_files = 0
    for crash_file in os.listdir(afl_crash_dir):

        if crash_file == "README.txt":
            continue
        
        print("Processing crash file: {}...".format(crash_file))
        crashfile_path = os.path.join(afl_crash_dir, crash_file)
        src_id = parse_src_from_file_name(crash_file)
        if src_id in original_img_orig_db:
            original_img = original_img_orig_db[src_id]
            print("Original image: {}".format(original_img))
        
            new_img_name = ",,,orig:{}".format(original_img)

            shutil.copy(crashfile_path, "analyzed_crashes/{}".format(new_img_name))
            
            command = ["fuzz/{}".format(afl_version), "analyzed_crashes/{}".format(new_img_name)]
            try:
                output = subprocess.run(command, capture_output=True, text=True, check=True)
                print("Command succeeded! (It shoudln't :( )")
            except subprocess.CalledProcessError as e:
                error_type, function_name = error_detector(e.stderr, crash_log)
                if not os.path.exists("analyzed_crashes/{}/{}".format(error_type, function_name)):
                    os.makedirs("analyzed_crashes/{}/{}".format(error_type, function_name))
                shutil.copy(crashfile_path, "analyzed_crashes/{}/{}/".format(error_type, function_name))
                count_processed_files += 1
            except Exception as e:
                print(f"An unexpected error occurred while trying to execute command: {e}")
        else:
            print("Error: could not find original image for crash file: {}".format(crash_file))
    print("Correctly processed files: {}.".format(count_processed_files))
    print("Crash log: \n\n")
    crash_log.print_crash_log()
            