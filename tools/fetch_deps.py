#!/usr/bin/env python3
"""Fetch AdMob dependencies for Android and iOS in V / Velo games.

Usage:
    python3 tools/fetch_deps.py [android|ios|all] [--project <path>]

Android:
    Downloads Google Play Services Ads SDK AAR/JAR dependencies into android/deps/
    and installs the AdBridge.java glue code.
iOS:
    Downloads Google Mobile Ads iOS SDK xcframework into ios/frameworks/
"""
import argparse
import json
import os
import re
import shutil
import sys
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
import zipfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODULE_ROOT = os.path.dirname(SCRIPT_DIR)

REPOS = ['https://dl.google.com/dl/android/maven2', 'https://repo.maven.apache.org/maven2']
IOS_SDK_URL = 'https://dl.google.com/googleadmobadssdk/googlemobileadssdkios.zip'

DEFAULT_CONFIG = {
    'dependencies': ['com.google.android.gms:play-services-ads:25.5.0'],
    'admob_app_id': 'ca-app-pub-3940256099942544~3347511713',
    'admob_app_id_ios': 'ca-app-pub-3940256099942544~1458002511',
}


def fetch_file(url, cache_dir):
    os.makedirs(cache_dir, exist_ok=True)
    filename = re.sub(r'[^A-Za-z0-9._-]', '_', url)
    dest = os.path.join(cache_dir, filename)
    if os.path.isfile(dest):
        return dest
    print(f'Downloading {url} ...')
    part = dest + '.part'
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'VeloAdMob/1.0'})
        with urllib.request.urlopen(req, timeout=120) as r, open(part, 'wb') as f:
            shutil.copyfileobj(r, f)
    except urllib.error.HTTPError as e:
        if e.code == 404:
            if os.path.exists(part):
                os.remove(part)
            return None
        raise
    os.replace(part, dest)
    return dest


def repo_file(group, artifact, version, ext, cache_dir):
    rel = f"{group.replace('.', '/')}/{artifact}/{version}/{artifact}-{version}.{ext}"
    for repo in REPOS:
        p = fetch_file(f'{repo}/{rel}', cache_dir)
        if p:
            return p
    return None


def strip_ns(root):
    for el in root.iter():
        if '}' in el.tag:
            el.tag = el.tag.split('}', 1)[1]
    return root


class Pom:
    def __init__(self, group, artifact, version, cache_dir):
        self.group, self.artifact, self.version, self.cache_dir = group, artifact, version, cache_dir
        path = repo_file(group, artifact, version, 'pom', cache_dir)
        if not path:
            raise SystemExit(f'no POM for {group}:{artifact}:{version}')
        self.root = strip_ns(ET.parse(path).getroot())
        self.parent = None
        p = self.root.find('parent')
        if p is not None:
            self.parent = Pom(p.findtext('groupId'), p.findtext('artifactId'), p.findtext('version'), cache_dir)
        self.props = dict(self.parent.props) if self.parent else {}
        props = self.root.find('properties')
        if props is not None:
            for el in props:
                self.props[el.tag] = (el.text or '').strip()
        self.props['project.version'] = self.root.findtext('version') or (self.parent.version if self.parent else version)
        self.props['project.groupId'] = self.root.findtext('groupId') or (self.parent.group if self.parent else group)
        self.managed = dict(self.parent.managed) if self.parent else {}
        for d in self.root.findall('dependencyManagement/dependencies/dependency'):
            v = d.findtext('version')
            if v:
                self.managed[(d.findtext('groupId'), d.findtext('artifactId'))] = self.sub(v)
        self.packaging = (self.root.findtext('packaging') or 'jar').strip()

    def sub(self, s):
        s = s.strip()
        for _ in range(5):
            s2 = re.sub(r'\$\{([^}]+)\}', lambda m: self.props.get(m.group(1), m.group(0)), s)
            if s2 == s:
                break
            s = s2
        return s

    def dependencies(self):
        out = []
        for d in self.root.findall('dependencies/dependency'):
            scope = (d.findtext('scope') or 'compile').strip()
            if scope in ('test', 'provided', 'system'):
                continue
            if (d.findtext('optional') or '').strip().lower() == 'true':
                continue
            g = self.sub(d.findtext('groupId'))
            a = self.sub(d.findtext('artifactId'))
            v = d.findtext('version')
            v = self.sub(v) if v else self.managed.get((g, a))
            if not v:
                continue
            p = (d.findtext('type') or 'jar').strip()
            out.append((g, a, v, p))
        return out


def resolve_dependencies(roots, cache_dir):
    seen, order = set(), []

    def visit(group, artifact, version, packaging):
        key = (group, artifact)
        if key in seen:
            return
        seen.add(key)
        pom = Pom(group, artifact, version, cache_dir)
        for dep in pom.dependencies():
            visit(*dep)
        order.append((group, artifact, version, pom.packaging))

    for item in roots:
        parts = item.split(':')
        visit(parts[0], parts[1], parts[2], 'jar')
    return order


def element_id(el):
    tag = el.tag
    name = el.attrib.get('{http://schemas.android.com/apk/res/android}name', '')
    return (tag, name)


def serialize(el, indent=1):
    pad = '\t' * indent
    name = el.tag
    attrs = ''.join(f' {k}="{v}"' for k, v in sorted(el.attrib.items()))
    children = list(el)
    if not children and not (el.text or '').strip():
        return f'{pad}<{name}{attrs}/>\n'
    out = f'{pad}<{name}{attrs}>\n'
    for c in children:
        out += serialize(c, indent + 1)
    out += f'{pad}</{name}>\n'
    return out


def merge_manifest(manifest_files):
    top, app = {}, {}
    for path in manifest_files:
        root = ET.parse(path).getroot()
        for el in root:
            if el.tag in ('uses-permission', 'uses-feature', 'permission'):
                key = (el.tag, element_id(el))
                top.setdefault(key, (el, None))
            elif el.tag == 'queries':
                for child in el:
                    top.setdefault(('queries', element_id(child), ''), (child, 'queries'))
            elif el.tag == 'application':
                for child in el:
                    first = app.setdefault(element_id(child), child)
                    if first is not child:
                        have = {element_id(k) for k in first}
                        first.extend(k for k in child if element_id(k) not in have)
    perms, queries = '', ''
    for key, (el, wrap) in top.items():
        if wrap == 'queries':
            queries += serialize(el, 2)
        else:
            perms += serialize(el, 1)
    if queries:
        perms += '\t<queries>\n' + queries + '\t</queries>\n'
    return perms, ''.join(serialize(el, 2) for el in app.values())


def aapt1_values(data):
    text = data.decode('utf-8')
    text = re.sub(r'<id\s+name="([^"]+)"\s*/>', r'<item type="id" name="\1"/>', text)
    return text.encode('utf-8')


def setup_android(project_root):
    print('=== Setting up AdMob for Android ===')
    deps_dir = os.path.join(project_root, 'android', 'deps')
    cache_dir = os.path.join(project_root, 'android', '.cache')
    config_file = os.path.join(project_root, 'android', 'deps.json')

    cfg = dict(DEFAULT_CONFIG)
    if os.path.isfile(config_file):
        with open(config_file, 'r') as f:
            cfg.update(json.load(f))
    else:
        os.makedirs(os.path.dirname(config_file), exist_ok=True)
        with open(config_file, 'w') as f:
            json.dump(cfg, f, indent=2)
            f.write('\n')

    libs = resolve_dependencies(cfg['dependencies'], cache_dir)
    print(f'Resolved {len(libs)} libraries')

    shutil.rmtree(deps_dir, ignore_errors=True)
    for d in ('jars', 'res', 'src'):
        os.makedirs(os.path.join(deps_dir, d), exist_ok=True)

    packages, manifests = [], []
    for n, (g, a, v, packaging) in enumerate(libs):
        ext = 'aar' if packaging == 'aar' else 'jar'
        path = repo_file(g, a, v, ext, cache_dir)
        if not path:
            raise SystemExit(f'Cannot download {g}:{a}:{v}.{ext}')
        tag = f'{g}_{a}-{v}'
        if ext == 'jar':
            shutil.copy(path, os.path.join(deps_dir, 'jars', tag + '.jar'))
            continue
        with zipfile.ZipFile(path) as z:
            names = z.namelist()
            if 'classes.jar' in names:
                with z.open('classes.jar') as src, open(os.path.join(deps_dir, 'jars', tag + '.jar'), 'wb') as dst:
                    shutil.copyfileobj(src, dst)
            for name in names:
                if name.startswith('libs/') and name.endswith('.jar'):
                    with z.open(name) as src, open(os.path.join(deps_dir, 'jars', f'{tag}_{os.path.basename(name)}'), 'wb') as dst:
                        shutil.copyfileobj(src, dst)
            res = [x for x in names if x.startswith('res/') and not x.endswith('/')]
            if res:
                dest = os.path.join(deps_dir, 'res', f'{n:02d}_{a}')
                for x in res:
                    target = os.path.join(dest, x[len('res/'):])
                    os.makedirs(os.path.dirname(target), exist_ok=True)
                    with z.open(x) as src, open(target, 'wb') as dst:
                        data = src.read()
                        if x.startswith('res/values') and x.endswith('.xml'):
                            data = aapt1_values(data)
                        dst.write(data)
            if 'AndroidManifest.xml' in names:
                mf = os.path.join(cache_dir, f'manifest_{tag}.xml')
                with z.open('AndroidManifest.xml') as src, open(mf, 'wb') as dst:
                    shutil.copyfileobj(src, dst)
                manifests.append(mf)
                pkg = ET.parse(mf).getroot().get('package')
                if pkg and res:
                    packages.append(pkg)

    perms, application = merge_manifest(manifests)
    app_id = cfg.get('admob_app_id', DEFAULT_CONFIG['admob_app_id'])
    application = f'\t\t<meta-data android:name="com.google.android.gms.ads.APPLICATION_ID" android:value="{app_id}"/>\n' + application
    perms = ''.join(dict.fromkeys(perms.splitlines(True)))
    with open(os.path.join(deps_dir, 'manifest_permissions.xml'), 'w') as f:
        f.write(perms)
    with open(os.path.join(deps_dir, 'manifest_application.xml'), 'w') as f:
        f.write(application)
    with open(os.path.join(deps_dir, 'extra_packages.txt'), 'w') as f:
        f.write('\n'.join(sorted(set(packages))) + '\n')

    # Install AdBridge.java from module
    module_bridge = os.path.join(MODULE_ROOT, 'android', 'AdBridge.java')
    target_bridge_dir = os.path.join(deps_dir, 'src', 'io', 'velo', 'admob')
    os.makedirs(target_bridge_dir, exist_ok=True)
    if os.path.isfile(module_bridge):
        shutil.copy(module_bridge, os.path.join(target_bridge_dir, 'AdBridge.java'))
        print(f'Installed AdBridge.java to {target_bridge_dir}')

    # Also support project's android/glue if exists
    project_glue = os.path.join(project_root, 'android', 'glue')
    if os.path.isdir(project_glue):
        shutil.copytree(project_glue, os.path.join(deps_dir, 'src'), dirs_exist_ok=True)

    print(f'Android AdMob setup complete -> {os.path.relpath(deps_dir, project_root)}')


def setup_ios(project_root):
    print('=== Setting up AdMob for iOS ===')
    cache_dir = os.path.join(project_root, 'ios', '.cache')
    dest_dir = os.path.join(project_root, 'ios', 'frameworks')
    os.makedirs(dest_dir, exist_ok=True)

    zip_path = fetch_file(IOS_SDK_URL, cache_dir)
    if not zip_path or not os.path.isfile(zip_path):
        raise SystemExit('Failed to download Google Mobile Ads iOS SDK')

    print('Extracting Google Mobile Ads xcframeworks...')
    with zipfile.ZipFile(zip_path) as z:
        for member in z.infolist():
            # Extract .xcframework bundles
            m = re.search(r'([^/]+\.xcframework)/?(.*)', member.filename)
            if m:
                framework_name = m.group(1)
                sub_path = m.group(2)
                out_path = os.path.join(dest_dir, framework_name, sub_path)
                if member.is_dir():
                    os.makedirs(out_path, exist_ok=True)
                else:
                    os.makedirs(os.path.dirname(out_path), exist_ok=True)
                    with z.open(member) as src, open(out_path, 'wb') as dst:
                        shutil.copyfileobj(src, dst)

    print(f'iOS AdMob setup complete -> {os.path.relpath(dest_dir, project_root)}')


def main():
    parser = argparse.ArgumentParser(description='Fetch AdMob dependencies for Android and iOS')
    parser.add_argument('platform', nargs='?', default='all', choices=['android', 'ios', 'all'])
    parser.add_argument('--project', default=os.getcwd(), help='Root path of the game project')
    args = parser.parse_args()

    project_root = os.path.abspath(args.project)
    if args.platform in ('android', 'all'):
        setup_android(project_root)
    if args.platform in ('ios', 'all'):
        setup_ios(project_root)


if __name__ == '__main__':
    main()
