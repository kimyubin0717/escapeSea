from pathlib import Path
import runpy

folder=Path(__file__).parent
runpy.run_path(str(folder/'organize_abyssal_assets.py'),run_name='__main__')
runpy.run_path(str(folder/'finalize_abyssal_city.py'),run_name='__main__')
