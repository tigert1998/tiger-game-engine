import os.path as osp
import requests
import urllib.parse
import re
import os
from zipfile import ZipFile

THIRD_PARTY_PATH = "third_party"


def generate_glad_url():
    origin = "https://glad.dav1d.de"
    data = "language=c&specification=gl&api=gl%3D4.6&api=gles1%3Dnone&api=gles2%3Dnone&api=glsc2%3Dnone&profile=core&extensions=GL_ARB_bindless_texture&extensions=GL_NV_shader_atomic_fp16_vector&loader=on"
    response = requests.post(
        url=urllib.parse.urljoin(origin, "generate"),
        data=data,
        headers={
            "Content-Type": "application/x-www-form-urlencoded",
        },
    )
    html = response.text
    span = re.search("/generated/.*/glad.zip", html).span()
    glad_url = urllib.parse.urljoin(origin, html[span[0] : span[1]])
    print("Generate glad zip url: {}".format(glad_url))
    return glad_url


def main():
    glad_url = generate_glad_url()
    r = requests.get(glad_url)
    glad_zip_path = osp.join(THIRD_PARTY_PATH, "glad.zip")
    glad_path = osp.join(THIRD_PARTY_PATH, "glad")
    with open(glad_zip_path, "wb") as f:
        f.write(r.content)

    ZipFile(glad_zip_path).extractall(glad_path)
    print("Unzip glad to: {}".format(glad_path))
    os.remove(glad_zip_path)


if __name__ == "__main__":
    main()
