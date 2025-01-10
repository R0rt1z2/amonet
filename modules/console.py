import enum
import os
import readline
import sys

from functions import hexdump


class Mode(enum.Enum):
    BOOTROM = 0
    PRELOADER = 1
    PAYLOAD = 2


class Console:
    def __init__(self, dev):
        self.dev = dev
        self.mode = Mode.PAYLOAD

        self.history_file = os.path.expanduser('~/.chistory')
        self.load_history()

        print(
            '\nWelcome to the interactive console!\n'
            "Type 'help' to get started. To exit,\n"
            "type 'exit' and press Enter. Have fun!"
        )

    def prompt(self):
        return input('(%s) > ' % self.mode.name.lower())

    def process_cmd(self, user_input):
        try:
            parts = user_input.strip().split()
            if not parts:
                return
            command, args = parts[0], parts[1:]

            method_name = f'cmd_{command}'
            if hasattr(self, method_name):
                method = getattr(self, method_name)
                method(*args)
            else:
                print(
                    "Unknown command: %s. Type 'help' for a list of available commands."
                    % command
                )
        except OSError as e:
            if any(
                s in str(e).lower()
                for s in ['device disconnected', 'input/output error']
            ):
                print('I/O error. Device most likely disconnected.')
                sys.exit(1)
            else:
                print(f'Unexpected OS error: %s' % e)
        except Exception as e:
            if 'serial' in str(type(e)).lower() or 'device' in str(e).lower():
                print('Serial port error. Device most likely disconnected.')
                sys.exit(1)
            else:
                print(f'Unexpected error: {e}')
        finally:
            sys.stdout.flush()

    def run(self):
        while True:
            try:
                user_input = self.prompt()
                self.process_cmd(user_input)
            except KeyboardInterrupt:
                print("\nTo exit, type 'exit' and press Enter.")
                continue
            except EOFError:
                break

        self.save_history()

    def load_history(self):
        try:
            readline.read_history_file(self.history_file)
        except FileNotFoundError:
            pass

    def save_history(self):
        try:
            readline.write_history_file(self.history_file)
        except Exception as e:
            print('Error saving history: %s' % e)

    def die(self, code):
        try:
            print('Rebooting device...')
            if self.mode == Mode.PAYLOAD:
                self.dev.payload_reboot()
            elif self.mode == Mode.PRELOADER:
                self.dev.reboot()
        except Exception:
            pass
        sys.exit(code)

    def cmd_readmem(self, *args):
        """Reads memory from the device."""
        if len(args) < 2 or len(args) > 3:
            print('Usage: readmem <addr> <size> [<filename>]')
            return
        try:
            addr = int(args[0], 16)
            size = int(args[1], 16)

            data = self.dev.mem_read(addr, size)
            hexdump(data, addr)

            if len(args) == 3:
                filename = args[2]
                with open(filename, 'wb') as f:
                    f.write(data)
                print(f'Memory saved to {filename}')
        except ValueError:
            print('Invalid arguments')
        except Exception as e:
            raise e

    def cmd_writemem(self, *args):
        """Writes memory to the device."""
        if len(args) < 2:
            print('Usage: writemem <addr> <data>')
            return
        try:
            addr = int(args[0], 16)

            if len(args) == 2 and args[1].startswith('0x'):
                data = int(args[1], 16).to_bytes(4, byteorder='big')
            else:
                data = bytes.fromhex(''.join(args[1:]))

            self.dev.mem_write(addr, data)
            print('Memory written successfully.')
        except ValueError:
            print('Invalid arguments')
        except Exception as e:
            print(f'Error: {e}')

    def cmd_help(self, *args):
        """Shows available commands"""
        print(
            '\n'.join(
                f"{m[4:]}: {getattr(self, m).__doc__ or '??'}"
                for m in dir(self)
                if m.startswith('cmd_')
            )
        )

    def cmd_exit(self, *args):
        """Reboots the device and exits the console."""
        self.die(0)

    def cmd_reboot(self, *args):
        """Reboots the device and exits the console."""
        self.die(0)

    def cmd_clear(self, *args):
        """Clears the console."""
        os.system('cls' if os.name == 'nt' else 'clear')
