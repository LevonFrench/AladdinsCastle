# SPDX-License-Identifier: GPL-3.0-only
"""Reject hardcoded QML names and prove generated identity agreement without XR."""
import json, pathlib, re, shutil, subprocess, sys, tempfile
root=pathlib.Path(__file__).resolve().parents[2]
display_name=json.loads((root/'data/product.json').read_text(encoding='utf-8'))['display_name']
hardcoded_labels=[]
for qml in sorted((root/'hub/qml').rglob('*.qml')):
    for number,line in enumerate(qml.read_text(encoding='utf-8').splitlines(),1):
        if re.search(r'\btext\s*:\s*[\"\']'+re.escape(display_name),line):
            hardcoded_labels.append(f'{qml.relative_to(root)}:{number}')
assert not hardcoded_labels,'Hardcoded product display labels: '+', '.join(hardcoded_labels)
with tempfile.TemporaryDirectory() as temp:
    p=pathlib.Path(temp); hub=p/'hub';data=p/'data';data.mkdir();(hub/'cmake').mkdir(parents=True)
    for relative in ['cmake/ProductIdentity.cmake','src/core/app/ProductIdentity.h.in','resources/dashboard.vrmanifest.in']:
        destination=hub/relative;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/'hub'/relative,destination)
    product=json.loads((root/'data/product.json').read_text(encoding='utf-8'))
    product.update(display_name='Synthetic Product',short_id='synthetic-product',program_name='synthetic-program',overlay_key='org.synthetic.product',steam_shortcut_name='SyntheticShortcut',user_data_folder='SyntheticData',dsu_server_name='SyntheticDSU')
    (data/'product.json').write_text(json.dumps(product),encoding='utf-8')
    (hub/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.28)\nproject(IdentityFixture LANGUAGES NONE)\ninclude(cmake/ProductIdentity.cmake)\nconfigure_file(resources/dashboard.vrmanifest.in "${CMAKE_BINARY_DIR}/dashboard.vrmanifest" @ONLY)\n',encoding='utf-8')
    command=[sys.argv[1],'-S',str(hub),'-B',str(p/'build')]
    result=subprocess.run(command,capture_output=True,text=True);assert result.returncode==0,result.stdout+result.stderr
    header=(p/'build/generated/ProductIdentity.h').read_text();assert all(value in header for field,value in product.items() if field!='schema')
    manifest=json.loads((p/'build/dashboard.vrmanifest').read_text())['applications'][0]
    assert manifest['app_key']==product['overlay_key']
    assert manifest['binary_path_windows']=='../'+product['program_name']+'.exe'
    assert manifest['strings']['en_us']['name']==product['display_name']
    assert (p/'build/generated/product.json').read_bytes()==(data/'product.json').read_bytes()
    product['schema']=2;(data/'product.json').write_text(json.dumps(product))
    result=subprocess.run(command,capture_output=True,text=True);assert result.returncode!=0,'Unsupported identity schema admitted'
print('PASS QML display-name guard, alternate product identity, manifest/program agreement, exact snapshot and invalid-schema refusal')
