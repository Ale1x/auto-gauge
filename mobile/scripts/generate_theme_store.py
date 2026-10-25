#!/usr/bin/env python3
"""Generate local StoreKit test products from the one catalog source."""
import json
import hashlib
from pathlib import Path
root = Path(__file__).resolve().parents[1]
themes = json.loads((root/'Sources/GaugeCore/Resources/themes.json').read_text())
products = []
for theme in themes:
    if theme['id'] == 'original': continue
    products.append({'displayPrice':'1.99', 'familyShareable':False, 'internalID':str(int(hashlib.sha256(theme['id'].encode()).hexdigest()[:10],16)), 'localizations':[{'description':theme['summary'],'displayName':theme['title'],'locale':'it_IT'}], 'productID':'it.ale1x.autogauge.theme.'+theme['id'], 'referenceName':theme['title'], 'type':'NonConsumable'})
value={'identifier':'A58F251A-550A-4F3A-829C-44832A85A833','nonRenewingSubscriptions':[],'products':products,'settings':{'_failTransactionsEnabled':False,'_locale':'it_IT','_storefront':'ITA'},'subscriptionGroups':[],'version':{'major':3,'minor':0}}
(root/'Config/Themes.storekit').write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
