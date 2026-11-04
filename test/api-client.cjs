/* Firmware REST client contract in jsdom: CSRF, dashboard persistence and command dispatch. */
const {JSDOM}=require('jsdom');
const fs=require('node:fs');
const assert=require('node:assert/strict');
const html=fs.readFileSync('data/index.html','utf8'), js=fs.readFileSync('data/app.js','utf8');
let dashboard={name:'Home',published:false,widgets:[]}, value=0, writes=0, commands=0;
const device={id:'fixture',name:'Fixture relay',type:'relay',pin:14,value:0,enabled:true};
const response=(data,status=200)=>({ok:status<400,status,json:async()=>JSON.parse(JSON.stringify(data))});
const dom=new JSDOM(html,{url:'http://esp32.local/',runScripts:'dangerously',pretendToBeVisual:true,beforeParse(win){
  win.setInterval=()=>0;
  win.Element.prototype.scrollIntoView=()=>{};
  win.fetch=async(url,opts={})=>{
    const path=new URL(url,win.location.href).pathname,method=opts.method||'GET';
    if(path==='/api/bootstrap')return response({setup:false,version:'0.1.0-preview'});
    if(path==='/api/auth/session')return response({authenticated:true,csrf:'fixture-csrf'});
    if(path==='/api/ws-ticket')return response({error:{message:'WS unavailable'}},503);
    if(path==='/api/system/status')return response({name:'Test',version:'0.1.0-preview',wifiConnected:true,deviceCount:1,ruleCount:0});
    if(path==='/api/devices'&&method==='GET')return response([{...device,value}]);
    if(['/api/scenes','/api/schedules','/api/automations','/api/events'].includes(path))return response([]);
    if(path==='/api/dashboard'&&method==='GET')return response(dashboard);
    if(path==='/api/dashboard'&&method==='PUT'){
      assert.equal(opts.headers['X-CSRF-Token'],'fixture-csrf');
      dashboard=JSON.parse(opts.body);writes++;return response(dashboard);
    }
    if(path==='/api/devices/fixture/command'&&method==='POST'){
      assert.equal(opts.headers['X-CSRF-Token'],'fixture-csrf');
      assert.equal(JSON.parse(opts.body).command,'toggle');
      value=Number(!value);commands++;return response({ok:true});
    }
    throw Error('Unexpected route '+method+' '+path);
  };
}});
const tick=()=>new Promise(resolve=>setTimeout(resolve,30));
const click=selector=>{let element=dom.window.document.querySelector(selector);assert.ok(element,'Missing '+selector);element.click()};
(async()=>{
  dom.window.eval(js);await tick();
  assert.match(dom.window.document.body.textContent,/Local network/);
  click('.sidebar [data-nav="designer"]');click('[data-widget-type="toggle"]');
  let inspector=dom.window.document.querySelector('[data-prop="deviceId"]');inspector.value='fixture';
  inspector.dispatchEvent(new dom.window.Event('change',{bubbles:true}));
  click('[data-action="publish"]');await tick();
  assert.equal(writes,1);
  assert.equal(dashboard.widgets[0].deviceId,'fixture');
  assert.ok(dom.window.document.querySelector('.published-grid [data-command="fixture"]'));
  click('.published-grid [data-command="fixture"]');await tick();
  assert.equal(commands,1);assert.equal(value,1);
  assert.deepEqual(dashboard.widgets.length,1);
  console.log('Firmware-mode API client test passed: authenticated publish, mapping, CSRF and command.');
})().catch(error=>{console.error(error);process.exitCode=1}).finally(()=>dom.window.close());
