// Browser-level contract test for the firmware REST mode, not the localStorage demo.
// Serve data/ on BASE_URL; CHROMIUM_PATH=/path/to/chromium npm run test:api-browser
const { chromium } = require('playwright-core');
const assert = require('node:assert/strict');
const base = process.env.BASE_URL || 'http://127.0.0.1:4173';
if (!process.env.CHROMIUM_PATH) throw Error('Set CHROMIUM_PATH to Chromium.');
(async () => {
  const browser=await chromium.launch({executablePath:process.env.CHROMIUM_PATH,args:['--no-sandbox','--disable-dev-shm-usage']});
  const page=await browser.newPage({viewport:{width:1440,height:900}});
  const errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  let dashboard={name:'Home',published:false,widgets:[]};
  let output={id:'relay01',name:'Fixture relay',type:'relay',pin:14,value:0,enabled:true,room:'Lab'};
  let commands=0, dashboardWrites=0;
  const json=(route,body,status=200)=>route.fulfill({status,contentType:'application/json',body:JSON.stringify(body)});
  await page.route('**/api/**',async route=>{
    const request=route.request(), method=request.method(), pathname=new URL(request.url()).pathname;
    if(pathname==='/api/bootstrap')return json(route,{setup:false,version:'0.1.0-preview'});
    if(pathname==='/api/auth/session')return json(route,{authenticated:true,csrf:'test-csrf'});
    if(pathname==='/api/ws-ticket')return json(route,{error:{message:'WebSocket offline in fixture'}},503);
    if(pathname==='/api/devices'&&method==='GET')return json(route,[output]);
    if(pathname==='/api/automations'||pathname==='/api/scenes'||pathname==='/api/schedules'||pathname==='/api/events')return json(route,[]);
    if(pathname==='/api/system/status')return json(route,{name:'Test lab',version:'0.1.0-preview',wifiConnected:true,ip:'192.168.1.50',uptimeSeconds:180,freeHeap:90000,freePsram:4000000});
    if(pathname==='/api/dashboard'&&method==='GET')return json(route,dashboard);
    if(pathname==='/api/dashboard'&&method==='PUT'){
      assert.equal(request.headers()['x-csrf-token'],'test-csrf');
      dashboard=JSON.parse(request.postData());dashboardWrites++;return json(route,dashboard);
    }
    if(pathname==='/api/devices/relay01/command'&&method==='POST'){
      assert.equal(request.headers()['x-csrf-token'],'test-csrf');
      const {command}=JSON.parse(request.postData());assert.equal(command,'toggle');
      output.value=Number(!output.value);commands++;return json(route,{ok:true});
    }
    return json(route,{error:{code:'not_found',message:pathname}},404);
  });
  try{
    await page.goto(base);
    await page.locator('.hero').waitFor();
    assert.equal(await page.locator('.status-chip').innerText(),'Local network');
    await page.locator('.sidebar [data-nav="designer"]').click();
    await page.locator('[data-widget-type="toggle"]').click();
    await page.locator('[data-prop="title"]').fill('Fixture switch');
    await page.locator('[data-prop="title"]').dispatchEvent('change');
    await page.locator('[data-prop="deviceId"]').selectOption('relay01');
    await page.locator('[data-action="publish"]').click();
    await page.locator('.published-grid').waitFor();
    assert.equal(dashboardWrites,1);
    assert.equal(dashboard.widgets[0].deviceId,'relay01');
    await page.locator('[data-published-widget] [data-command="relay01"]').click();
    assert.equal(commands,1);
    await page.reload();
    await page.locator('.published-grid').waitFor();
    assert.match(await page.locator('.published-grid').innerText(),/Fixture switch/);
    assert.equal(await page.locator('[data-published-widget] [data-command="relay01"]').getAttribute('aria-pressed'),'true');
    assert.deepEqual(errors,[]);
    console.log('Real API browser contract passed: session, CSRF, dashboard publish/GET, command and reload.');
  }finally{await browser.close()}
})().catch(error=>{console.error(error);process.exitCode=1});
