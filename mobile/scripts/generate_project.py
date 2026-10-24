#!/usr/bin/env python3
"""Generate the development Xcode project using only the Python standard library."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(ROOT/"scripts/generate_demo_logs.py")], check=True)
objects = {}
def uid(name):
    return hashlib.sha256(name.encode()).hexdigest()[:24].upper()
def obj(object_key, **fields):
    key = uid(object_key)
    objects[key] = fields
    return key

def render(value):
    if isinstance(value, dict):
        return '{\n' + '\n'.join(f'{json.dumps(k)} = {render(v)};' for k,v in value.items()) + '\n}'
    if isinstance(value, list):
        return '(' + ', '.join(render(v) for v in value) + ')'
    return json.dumps(value)

file = obj('app-file', isa='PBXFileReference', lastKnownFileType='sourcecode.swift', path='App/AutoGaugeApp.swift', sourceTree='<group>')
product = obj('app-product', isa='PBXFileReference', explicitFileType='wrapper.application', includeInIndex='0', path='AutoGauge.app', sourceTree='BUILT_PRODUCTS_DIR')
products = obj('products', isa='PBXGroup', children=[product], name='Products', sourceTree='<group>')
root_group = obj('root-group', isa='PBXGroup', children=[file, products], sourceTree='<group>')
package = obj('package', isa='XCLocalSwiftPackageReference', relativePath='.')
package_product = obj('package-product', isa='XCSwiftPackageProductDependency', package=package, productName='GaugeCore')
app_source = obj('app-source', isa='PBXBuildFile', fileRef=file)
core_link = obj('core-link', isa='PBXBuildFile', productRef=package_product)
sources = obj('sources', isa='PBXSourcesBuildPhase', buildActionMask='2147483647', files=[app_source], runOnlyForDeploymentPostprocessing='0')
frameworks = obj('frameworks', isa='PBXFrameworksBuildPhase', buildActionMask='2147483647', files=[core_link], runOnlyForDeploymentPostprocessing='0')
resources = obj('resources', isa='PBXResourcesBuildPhase', buildActionMask='2147483647', files=[], runOnlyForDeploymentPostprocessing='0')
project_configs = []
target_configs = []
for mode in ('Debug', 'Release'):
    project_configs.append(obj('project-'+mode, isa='XCBuildConfiguration', name=mode, buildSettings={
        'SDKROOT': 'iphoneos', 'IPHONEOS_DEPLOYMENT_TARGET': '17.0', 'SWIFT_VERSION': '5.0',
        'CLANG_ENABLE_MODULES': 'YES', 'SWIFT_OPTIMIZATION_LEVEL': '-Onone' if mode == 'Debug' else '-O',
    }))
    target_configs.append(obj('target-'+mode, isa='XCBuildConfiguration', name=mode, buildSettings={
        'PRODUCT_NAME': 'AutoGauge', 'PRODUCT_BUNDLE_IDENTIFIER': 'local.autogauge.demo',
        'GENERATE_INFOPLIST_FILE': 'YES', 'INFOPLIST_KEY_CFBundleDisplayName': 'Auto Gauge',
        'INFOPLIST_KEY_LSApplicationCategoryType': 'public.app-category.utilities',
        'INFOPLIST_KEY_UILaunchScreen_Generation': 'YES',
        'INFOPLIST_KEY_UIApplicationSceneManifest_Generation': 'YES',
        'TARGETED_DEVICE_FAMILY': '1', 'SUPPORTED_PLATFORMS': 'iphoneos iphonesimulator',
        'CODE_SIGNING_ALLOWED': 'NO', 'SWIFT_EMIT_LOC_STRINGS': 'NO',
        'SWIFT_ACTIVE_COMPILATION_CONDITIONS': 'DEBUG' if mode == 'Debug' else '',
    }))
project_list = obj('project-configs', isa='XCConfigurationList', buildConfigurations=project_configs, defaultConfigurationIsVisible='0', defaultConfigurationName='Release')
target_list = obj('target-configs', isa='XCConfigurationList', buildConfigurations=target_configs, defaultConfigurationIsVisible='0', defaultConfigurationName='Release')
target = obj('app-target', isa='PBXNativeTarget', buildConfigurationList=target_list, buildPhases=[sources,frameworks,resources], buildRules=[], dependencies=[], name='AutoGauge', productName='AutoGauge', productReference=product, productType='com.apple.product-type.application', packageProductDependencies=[package_product])
project = obj('project', isa='PBXProject', attributes={'BuildIndependentTargetsInParallel':'YES','LastUpgradeCheck':'2700'}, buildConfigurationList=project_list, compatibilityVersion='Xcode 14.0', developmentRegion='it', knownRegions=['it','Base'], mainGroup=root_group, productRefGroup=products, projectDirPath='', projectRoot='', targets=[target], packageReferences=[package])
folder = ROOT/'AutoGauge.xcodeproj'
(folder/'xcshareddata/xcschemes').mkdir(parents=True, exist_ok=True)
(folder/'project.pbxproj').write_text('// !$*UTF8*$!\n' + render({'archiveVersion':'1','classes':{},'objectVersion':'56','objects':objects,'rootObject':project}) + '\n')
(folder/'xcshareddata/xcschemes/AutoGauge.xcscheme').write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="2700" version="1.3">
 <BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="AutoGauge.app" BlueprintName="AutoGauge" ReferencedContainer="container:AutoGauge.xcodeproj"/></BuildActionEntry></BuildActionEntries></BuildAction>
 <TestAction buildConfiguration="Debug" shouldUseLaunchSchemeArgsEnv="YES"/>
 <LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="AutoGauge.app" BlueprintName="AutoGauge" ReferencedContainer="container:AutoGauge.xcodeproj"/></BuildableProductRunnable></LaunchAction>
 <ProfileAction buildConfiguration="Release" shouldUseLaunchSchemeArgsEnv="YES" savedToolIdentifier="" useCustomWorkingDirectory="NO" debugDocumentVersioning="YES"/>
 <AnalyzeAction buildConfiguration="Debug"/>
 <ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>
''')
print(folder)
