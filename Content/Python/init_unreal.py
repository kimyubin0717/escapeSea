"""Set the project editor's startup view without changing game or commandlet runs."""
import unreal

args=unreal.SystemLibrary.get_command_line().lower().split()
if "-game" not in args and not any(a.startswith(("-run=","-executepythonscript=")) for a in args):
    from mama_top_view import start_top_view
    start_top_view("-abysseditortopviewqa" in args)

