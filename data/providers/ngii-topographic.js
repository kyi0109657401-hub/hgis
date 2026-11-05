// NGII public contract inspected 2026-09-09. No credentials, tokens or file URLs.
// Called only in the trusted portal's MainWorld. Async results are polled as JSON.
(function (input) {
  'use strict';
  const result = (state, message, extra) => Object.assign({state, message: message || ''}, extra || {});
  const visible = e => {
    if(!e || !e.getClientRects().length)return false;
    for(let node=e;node;node=node.parentElement){const s=getComputedStyle(node);
      if(s.display==='none' || s.visibility==='hidden' || Number(s.opacity)===0)return false;}
    return true;
  };
  const changed = e => { e.dispatchEvent(new Event('input', {bubbles:true})); e.dispatchEvent(new Event('change', {bubbles:true})); };
  const button = (root, text) => Array.from(root.querySelectorAll('button,input[type=button],input[type=submit],a'))
    .filter(e => visible(e) && (e.textContent || e.value || '').trim() === text);
  const key = String(input.generation);
  const isDem = input.product === 'dem';
  if(input.command==='configureTransfer') {
    const local=input.testMode===true && location.protocol==='http:' && ['127.0.0.1','localhost'].includes(location.hostname);
    if(location.pathname!=='/nlippd/pd/innorix/innorixdownLoad.do' ||
        (!local && location.origin!=='https://nlippd.ngii.go.kr'))return result('ignored');
    if(!window.innorix || typeof window.innorix.create!=='function')return result('waitingFiles');
    // The official page constructs its control in window.onload. Select the
    // SDK's supported HTML5 transport before construction, including PCs with
    // no native Agent. Do not force readiness or bypass SDK validation.
    const create=window.innorix.create;
    window.innorix.create=function(options) {
      window.innorix.create=create;
      if(options && options.el==='#fileControl' && options.transferMode==='download')
        options=Object.assign({},options,{agent:false,showTransferWindow:false});
      return create.call(this,options);
    };
    return result('configured');
  }
  if (input.command === 'select') {
    let state = window.__kaTopographicSelection;
    if (!state || state.key !== key) {
      state = window.__kaTopographicSelection = {key, state:'querying',started:Date.now()};
      const post = (path, values) => fetch(path, {method:'POST', credentials:'same-origin',
        headers:{'Content-Type':'application/x-www-form-urlencoded; charset=UTF-8'}, body:new URLSearchParams(values),
        signal:AbortSignal.timeout(30000)}).then(r=>{if(!r.ok)throw new Error('http');return r.json();});
      // NGII coastal sheets may be shifted from their nominal sheet-number grid.
      // The official spatial intersection, including WKT, is authoritative.
      post('/ms/map/combineBufferQuery.do', {x:input.x,y:input.y,buffer:input.radius})
        .then(data=>{
          if(window.__kaTopographicSelection!==state)throw new Error('superseded');
          const area=data && data.items && data.items.length===1 && data.items[0];
          if(!area || typeof area.wkt!=='string' || !/^POLYGON\s*\(\(/i.test(area.wkt) ||
              area.wkt.length>100000 || ![area.minx,area.miny,area.maxx,area.maxy].every(Number.isFinite) ||
              Math.abs(area.minx-(input.x-input.radius))>1 || Math.abs(area.maxx-(input.x+input.radius))>1 ||
              Math.abs(area.miny-(input.y-input.radius))>1 || Math.abs(area.maxy-(input.y+input.radius))>1)throw new Error('scope');
          state.area=area;
          if(isDem) {
            return post('/ms/map/demYearsQuery.do',{}).catch(()=>({items:[{year:'2025'}]})).then(yearsData=>{
              if(window.__kaTopographicSelection!==state)throw new Error('superseded');
              const years=Array.isArray(yearsData && yearsData.items)
                ? yearsData.items.map(r=>String(r && r.year || '')).filter(y=>/^\d{4}$/.test(y)) : [];
              const maxYear=years.length?String(Math.max(...years.map(Number))):'2025';
              state.maxYear=maxYear;
              return post('/ms/map/demQuery.do',{wkt:area.wkt,minx:area.minx,miny:area.miny,
                maxx:area.maxx,maxy:area.maxy,toYear:'0',fromYear:maxYear});
            });
          }
          return post('/ms/map/suchiQuery.do',{index:'',type:'0',scale:'25000',wkt:area.wkt,
            minx:area.minx,miny:area.miny,maxx:area.maxx,maxy:area.maxy});
        })
        .then(data => {
          if (window.__kaTopographicSelection !== state) return;
          if (isDem) {
            const rawItems = data && data.items;
            if (!Array.isArray(rawItems) || rawItems.length>500) throw new Error('sheets');
            const byNum = new Map();
            for (const row of rawItems) {
              const num=String(row && row.num || ''), year=String(row && row.year || state.maxYear || '');
              const name=String(row && (row.nam || row.name) || '');
              if (!/^\d{5,6}$/.test(num) || !/^\d{4}$/.test(year) || !name ||
                  ![row.minx,row.miny,row.maxx,row.maxy].every(Number.isFinite) ||
                  row.minx>=row.maxx || row.miny>=row.maxy || row.maxx<state.area.minx || row.minx>state.area.maxx ||
                  row.maxy<state.area.miny || row.miny>state.area.maxy) throw new Error('product');
              const prev=byNum.get(num);
              if(!prev || Number(year)>Number(prev.year))
                byNum.set(num,{num,name,year,sigu:row.sigu?String(row.sigu):'',
                  minx:row.minx,miny:row.miny,maxx:row.maxx,maxy:row.maxy});
            }
            const items=Array.from(byNum.values()).slice(0,18);
            const wanted5=Array.from(new Set((input.numbers||[]).map(n=>String(n).slice(0,5))));
            state.unavailable=wanted5.filter(n=>!byNum.has(n));
            state.items=items;
            const cached=new Set((input.cachedNumbers||[]).map(n=>String(n).slice(0,5)));
            state.cached=items.filter(r=>cached.has(String(r.num))).map(r=>String(r.num));
            state.downloadItems=items.filter(r=>!cached.has(String(r.num)));
            if(!items.length){state.state='unavailable';return;}
            if(!state.downloadItems.length){state.state='cached';return;}
            state.state='waitingLayout';state.layoutStarted=Date.now();
            return;
          }
          const items = data && data.items;
          const actual = new Set();
          if (!Array.isArray(items) || items.length>100) throw new Error('sheets');
          for (const row of items) {
            if (!/^\d{6}$/.test(String(row.num)) || actual.has(String(row.num)) ||
                String(row.scale) !== '25000' || String(row.fileExt).toLowerCase() !== 'dxf' ||
                typeof row.name !== 'string' || !row.name || row.historyNo === undefined ||
                typeof row.projection !== 'string' || !row.projection ||
                ![row.minx,row.miny,row.maxx,row.maxy].every(Number.isFinite) ||
                row.minx>=row.maxx || row.miny>=row.maxy || row.maxx<state.area.minx || row.minx>state.area.maxx ||
                row.maxy<state.area.miny || row.miny>state.area.maxy) throw new Error('product');
            actual.add(String(row.num));
          }
          state.unavailable=input.numbers.filter(n=>!actual.has(n));
          state.items=items;
          const cached=new Set(input.cachedNumbers || []);
          state.cached=items.filter(r=>cached.has(String(r.num))).map(r=>String(r.num));
          state.downloadItems=items.filter(r=>!cached.has(String(r.num)));
          if(!items.length){state.state='unavailable';return;}
          if(!state.downloadItems.length){state.state='cached';return;}
          state.state='waitingLayout';state.layoutStarted=Date.now();
        }).catch(error => { if(window.__kaTopographicSelection===state) {
          const code=error && error.message;
          state.errorCode=['scope','product','layout','scale','http'].includes(code)?code:'http';
          state.state='blocked'; state.message='도엽·축척·파일 형식 또는 포털 구조를 확인하지 못했습니다. 필요한 도엽을 직접 선택해 주세요.';
        }});
    }
    if(state.state==='waitingLayout') {
      // The public spatial response may finish before NGII initializes its map
      // and product tabs. Reuse that response while waiting; never submit here.
      if (isDem) {
        const map=window.map,query=window.mapQuery,api=window.DEM_API;
        const panel=document.querySelector('#totalSearch section.mapList.dem');
        const view=map && typeof map.getView==='function' && map.getView();
        const projection=view && typeof view.getProjection==='function' && view.getProjection();
        const ready=projection && typeof projection.getCode==='function' && typeof view.setCenter==='function' &&
          query && typeof query.clear==='function' && api && typeof api.search==='function' &&
          typeof window.jQuery==='function' && window.integ && panel;
        state.errorCode='layout';
        if(!ready) {
          if(Date.now()-state.layoutStarted>60000){state.state='blocked';state.message='공식 지도의 초기화가 지연되고 있습니다. 공식 화면을 새로고침한 뒤 다시 시도해 주세요.';}
        } else if(projection.getCode()!=='EPSG:5179') {
          state.state='blocked';state.message='공식 지도의 좌표체계를 확인하지 못해 도엽 선택을 중단했습니다.';
        } else {
          try {
            query.clear();query.index=[];
            for(const field of ['wkt','minx','miny','maxx','maxy'])query[field]=state.area[field];
            window.integ.searchForm=3;
            view.setCenter([input.x,input.y]);
            const size=typeof map.getSize==='function' && map.getSize();
            if(Array.isArray(size) && size.length===2 && size.every(n=>Number.isFinite(n) && n>0) && typeof view.fit==='function')
              view.fit([input.x-input.radius,input.y-input.radius,input.x+input.radius,input.y+input.radius],size);
            panel.style.display='';
            const shell=document.querySelector('main.map.total');
            if(shell)shell.classList.add('_resultOn','_categoryOn','_mapListOn');
            const toSel=document.getElementById('toDemYear'),fromSel=document.getElementById('fromDemYear');
            if(toSel){if(!Array.from(toSel.options).some(o=>o.value==='0')){const opt=document.createElement('option');opt.value='0';opt.textContent='전체';toSel.appendChild(opt);}toSel.value='0';}
            if(fromSel && state.maxYear) {
              if(!Array.from(fromSel.options).some(o=>o.value===state.maxYear)){
                const opt=document.createElement('option');opt.value=state.maxYear;opt.textContent=state.maxYear+'년';
                fromSel.appendChild(opt);
              }
              fromSel.value=state.maxYear;
            }
            state.state='waitingRows';state.started=Date.now();
            api.search();
          } catch(error) {
            state.state='blocked';state.message='공식 DEM 도엽 목록을 준비하지 못했습니다. 공식 화면에서 새로고침한 뒤 다시 시도해 주세요.';
          }
        }
      } else {
        const map=window.map,query=window.mapQuery,api=window.MEASURE_API;
        const choice=document.querySelector('#totalSearch section.mapList.measure ._accordion>li>dl>dt button[value="0"]');
        const panel=document.querySelector('#totalSearch section.mapList.measure');
        const view=map && typeof map.getView==='function' && map.getView();
        const projection=view && typeof view.getProjection==='function' && view.getProjection();
        const scale=choice && choice.parentElement.querySelector('select');
        const ready=projection && typeof projection.getCode==='function' && typeof view.setCenter==='function' &&
          query && typeof query.clear==='function' && api && typeof api.completed==='function' &&
          typeof window.jQuery==='function' && window.integ && choice && panel;
        const scaleReady=scale && Array.from(scale.options).some(o=>o.value==='25000');
        state.errorCode=ready && !scaleReady?'scale':'layout';
        if(!ready || !scaleReady) {
          if(Date.now()-state.layoutStarted>60000){state.state='blocked';state.message='공식 지도의 초기화가 지연되고 있습니다. 공식 화면을 새로고침한 뒤 다시 시도해 주세요.';}
        } else if(projection.getCode()!=='EPSG:5179') {
          state.state='blocked';state.message='공식 지도의 좌표체계를 확인하지 못해 도엽 선택을 중단했습니다.';
        } else {
          try {
            query.clear();query.index=[];query.scale='25000';
            for(const field of ['wkt','minx','miny','maxx','maxy'])query[field]=state.area[field];
            window.integ.searchForm=3;
            if(window.simple && window.simple.settings)window.simple.settings.scale='25000';
            view.setCenter([input.x,input.y]);
            const size=typeof map.getSize==='function' && map.getSize();
            if(Array.isArray(size) && size.length===2 && size.every(n=>Number.isFinite(n) && n>0) && typeof view.fit==='function')
              view.fit([input.x-input.radius,input.y-input.radius,input.x+input.radius,input.y+input.radius],size);
            state.errorCode='scale';scale.value='25000';
            panel.style.display='';
            const shell=document.querySelector('main.map.total');
            if(shell)shell.classList.add('_resultOn','_categoryOn','_mapListOn');
            api.downArray=[];state.errorCode='layout';
            state.state='waitingRows';state.started=Date.now();
            api.completed('0',window.jQuery(choice));
          } catch(error) {
            state.state='blocked';state.message='공식 도엽 목록을 준비하지 못했습니다. 공식 화면에서 새로고침한 뒤 다시 시도해 주세요.';
          }
        }
      }
    }
    if (state.state === 'waitingRows') {
      if(Date.now()-state.started>60000){state.state='blocked';state.message='포털 도엽 목록을 기다리는 시간이 초과되었습니다. 공식 화면에서 목록을 확인해 주세요.';}
      if (isDem) {
        const all = Array.from(document.querySelectorAll('input[name="cb_dem"]'));
        const rows = state.downloadItems.map(item=>all.filter(e=>e.getAttribute('num')===String(item.num) &&
          e.getAttribute('year')===String(item.year)));
        if (state.state==='waitingRows' && rows.every(r=>r.length>=1)) {
          if(rows.some((r,i)=>(r[0].getAttribute('mapname') || r[0].getAttribute('nam'))!==state.downloadItems[i].name))
            return result('blocked','검색 자료와 신청 DEM 도엽의 이름이 다릅니다. 신청을 진행하지 않았습니다.');
          all.forEach(e=>{if(e.checked){e.checked=false;changed(e);}});
          rows.forEach(r=>{r[0].checked=true;changed(r[0]);});
          state.state='selected';
        }
      } else {
        const all = Array.from(document.querySelectorAll('input[name="cb_suchi"]'));
        const available=state.downloadItems.map(r=>String(r.num));
        const rows = available.map(n=>all.filter(e=>e.getAttribute('num')===n &&
          e.getAttribute('maptype')==='0' && e.getAttribute('scale')==='25000' && e.getAttribute('fileext')==='dxf'));
        if (state.state==='waitingRows' && rows.every(r=>r.length===1)) {
          if(rows.some((r,i)=>r[0].getAttribute('nam')!==state.downloadItems[i].name ||
              r[0].getAttribute('hisno')!==String(state.downloadItems[i].historyNo)))
            return result('blocked','검색 자료와 신청 도엽의 이름·이력이 다릅니다. 신청을 진행하지 않았습니다.');
          all.forEach(e=>{if(e.checked){e.checked=false;changed(e);}});
          rows.forEach(r=>{r[0].checked=true;changed(r[0]);});
          const selected = window.MEASURE_API.downArray;
          if (!Array.isArray(selected) || selected.length!==available.length || selected.some(r=>
            r.orderTp!=='0' || r.mapScale!=='25000' || r.ext!=='dxf' || !available.includes(r.mapNo)))
            return result('blocked','선택한 도엽과 포털 신청 목록이 다릅니다. 신청을 진행하지 않았습니다.');
          state.state='selected';
        }
      }
    }
    return result(state.state,state.message,{items:state.items || [],downloadItems:state.downloadItems || [],
      cached:state.cached || [],unavailable:state.unavailable || [],
      diagnostics:input.diagnostics===true && state.errorCode?{stage:'select',code:state.errorCode}:undefined});
  }
  if(input.command==='login') {
    const attempt=key+':'+input.loginAttempt;
    let wait=window.__kaLoginWait;
    if(!wait || wait.attempt!==attempt)wait=window.__kaLoginWait={attempt,started:Date.now()};
    // Login tabs take priority over openForm in the browser poller. Bound all
    // login routes too, including a rejected or unanswered hidden-iframe POST.
    if(Date.now()-wait.started>=300000)
      return result('blocked','로그인 응답 확인 시간이 초과되었습니다. 더보기의 수치지형도 계정 설정과 연결 상태를 확인한 뒤 다시 시도하세요.');
    // The official method chooser navigates its opener through this SSO
    // endpoint. Wait for its redirect; it is not an ID/password form.
    if(location.pathname==='/anyid/common/login.do')return result('waitingLogin');
    if(location.pathname==='/mn/loginPopup.do') {
      const choice=document.getElementById('ssoLogin');
      if(!visible(choice))return result('waitingLogin');
      if(window.__kaLoginMethodOpened!==attempt){window.__kaLoginMethodOpened=attempt;choice.click();}
      return result('waitingLogin');
    }
    if(location.pathname==='/anyid/common/anyidLogin.do') {
      const choice=document.querySelector('#loginContainer a.login-link.open-pop[data-target="UI_POT_01_107_wp"]');
      // NGII reveals loginContainer from ResizeObserver. A hidden WebEngine
      // view does not deliver that paint callback, even after AnyID has loaded.
      // The public ID method is also offered outside this container. Validate
      // its exact contract; CSS visibility is not authentication readiness.
      if(document.readyState==='loading' || !choice ||
          choice.getAttribute('href')!=='javascript:testFnLogin();' ||
          typeof window.testFnLogin!=='function')return result('waitingLogin');
      if(window.__kaIdMethodOpened!==attempt){window.__kaIdMethodOpened=attempt;choice.click();}
      return result('waitingLogin');
    }
    if(location.pathname!=='/member/login.do')return result('login','공식 인증 화면을 완료해 주세요.');
    // The URL becomes available before parser-blocking login scripts, the
    // form and its response iframe have arrived. This is preparation, not an
    // unsupported form. Keep the login deadline above active while waiting.
    if(document.readyState!=='complete')return result('waitingLogin');
    const form=document.getElementById('formLogin');
    const id=form && form.querySelector('input#mem_id[name="mem_id"]');
    const password=form && form.querySelector('input#mem_pw[name="mem_pw"][type="password"]');
    const action=form && new URL(form.action,location.href);
    if(!form || !id || !password || action.origin!==location.origin || action.pathname!=='/member/process.do' ||
        form.method.toLowerCase()!=='post' || form.target!=='hidden_iframe' || typeof window.fnFormValidation!=='function')
      return result('blocked','공식 로그인 양식이 예상과 달라 자동 로그인을 중단했습니다. 연결 상태를 확인한 뒤 다시 시도해 주세요.');
    // NGII's fnFormValidation closes over formLogin, initialized by jQuery's
    // ready callback after parsing. Do not fill credentials or mark an attempt
    // before that callback and the official hidden response target are ready.
    if(window.formLogin===null || !document.querySelector('iframe[name="hidden_iframe"]'))return result('waitingLogin');
    if(Array.from(document.querySelectorAll('input[name*="captcha" i],iframe[src*="captcha" i],.g-recaptcha')).some(visible))
      return result('login','보안문자 또는 추가 본인확인이 필요합니다. 공식 화면에서 완료해 주세요.');
    if(!input.loginId || !input.loginPassword)return result('input','더보기의 수치지형도 계정 설정에서 아이디와 비밀번호를 입력해 주세요.');
    if(window.__kaLoginAttempt===attempt)return result('waitingAuthentication');
    const submit=form.querySelector('input[type="submit"]');
    if(!submit)return result('blocked','공식 로그인 버튼을 확인하지 못했습니다.');
    id.value=input.loginId;changed(id);password.value=input.loginPassword;changed(password);
    window.__kaLoginAttempt=attempt;submit.click();
    return result('authenticationSent');
  }
  if (input.command === 'openForm') {
    const token=key+':'+input.number;
    let state=window.__kaTopographicOrder;
    if (!state || state.token!==token) {
      state=window.__kaTopographicOrder={token,state:'checkingLogin',loginStarted:Date.now(),
        nextLoginCheck:0,loginChecks:0,loginFailures:0,loginInFlight:false};
    }
    const awaitingLogin=state.state==='checkingLogin' || state.state==='login';
    if(awaitingLogin && !state.loginInFlight && Date.now()>=state.nextLoginCheck &&
        (state.loginChecks>=120 || Date.now()-state.loginStarted>=300000)) {
      state.state='blocked';state.message='로그인 확인 시간이 초과되었습니다. 계정 설정과 인증 상태를 확인한 뒤 다시 시도하세요.';
    }
    if((state.state==='checkingLogin' || state.state==='login') && !state.loginInFlight && Date.now()>=state.nextLoginCheck) {
      // An official login popup may authenticate and close without reloading the
      // map. Re-check this same order, with one request at a time and a deadline;
      // never re-open login or re-submit an application on every polling tick.
      state.loginInFlight=true;++state.loginChecks;state.state='checkingLogin';
      fetch('/mn/loginCheck.do',{method:'POST',credentials:'same-origin',signal:AbortSignal.timeout(30000)})
        .then(r=>{if(!r.ok)throw new Error('http');return r.json();})
        .then(data=>{
          if(window.__kaTopographicOrder!==state)return;
          if(!data || typeof data.isLogin!=='boolean')throw new Error('loginResponse');
          state.loginFailures=0;
          if(!data.isLogin){state.state='login';state.nextLoginCheck=Date.now()+1500;return;}
          state.state='openingForm';
          state.validationStep='metadata';
          const records=Array.isArray(input.records)?input.records:[input.record];
          if(!records.length || records.length>100 || records.some(r=>!r) ||
              new Set(records.map(r=>String(r.num))).size!==records.length)throw new Error('selection');
          if(isDem) {
            const all=Array.from(document.querySelectorAll('input[name="cb_dem"]'));
            state.validationStep='checkboxes';
            const checks=records.map(metadata=>{
              const matches=all.filter(e=>e.getAttribute('num')===String(metadata.num) &&
                e.getAttribute('year')===String(metadata.year));
              if(!matches.length || (matches[0].getAttribute('mapname') || matches[0].getAttribute('nam'))!==metadata.name)
                throw new Error('selection');
              return matches[0];
            });
            for(const check of all)if(check.checked && !checks.includes(check)){check.checked=false;changed(check);}
            for(const check of checks)if(!check.checked){check.checked=true;changed(check);}
            state.validationStep='rows';
            if(typeof window.downloadMapData!=='function')throw new Error('selection');
            const rows=records.map(metadata=>({
              types:(metadata.sigu && metadata.sigu!=='undefined')?'bound':'index',
              years:String(metadata.year),
              nums:String(metadata.num),
              names:String(metadata.name)
            }));
            const previous=document.querySelector('.popup.mapApplication.online input[name="onBrthdy"]');
            state.validationStep='modal';
            if(visible(previous))throw new Error('openForm');
            state.previousBirth=previous;
            state.state='waitingForm';
            state.validationStep='open';
            window.downloadMapData('4','online',rows,null);
          } else {
            const all=Array.from(document.querySelectorAll('input[name="cb_suchi"]'));
            state.validationStep='checkboxes';
            const checks=records.map(metadata=>{
              const matches=all.filter(e=>e.getAttribute('num')===String(metadata.num) &&
                e.getAttribute('maptype')==='0' && e.getAttribute('scale')==='25000' && e.getAttribute('fileext')==='dxf');
              if(matches.length!==1 || matches[0].getAttribute('nam')!==metadata.name ||
                  matches[0].getAttribute('hisno')!==String(metadata.historyNo))throw new Error('selection');
              return matches[0];
            });
            // One application contains every verified uncached sheet. Preserve
            // the records: closing the official form clears its selection array.
            for(const check of all)if(check.checked && !checks.includes(check)){check.checked=false;changed(check);}
            for(const check of checks)if(!check.checked){check.checked=true;changed(check);}
            const rows=window.MEASURE_API && window.MEASURE_API.downArray;
            state.validationStep='rows';
            if(!Array.isArray(rows) || rows.length!==records.length || records.some(metadata=>
                rows.filter(r=>r.mapNo===String(metadata.num) && r.mapName===metadata.name &&
                  String(r.mapHistoryNo)===String(metadata.historyNo) && r.orderTp==='0' && r.mapScale==='25000' && r.ext==='dxf').length!==1) ||
                typeof window.downloadMapData!=='function')throw new Error('selection');
            const previous=document.querySelector('.popup.mapApplication.online input[name="onBrthdy"]');
            state.validationStep='modal';
            if(visible(previous))throw new Error('openForm');
            state.previousBirth=previous;
            // Do not call fnCloseModal here: it also clears the verified selection.
            // setModalPopup replaces the old hidden body when the new response arrives.
            state.state='waitingForm';
            state.validationStep='open';
            window.downloadMapData('1','online',rows.slice(),null);
          }
        }).catch(()=>{
          if(window.__kaTopographicOrder!==state)return;
          if(state.state==='checkingLogin' && ++state.loginFailures<3){
            state.nextLoginCheck=Date.now()+1500*state.loginFailures;
            return;
          }
          state.state='blocked';state.message='공식 로그인 또는 신청 화면을 확인하지 못했습니다. 연결 상태를 확인한 뒤 다시 시도하세요.';
        }).finally(()=>{if(window.__kaTopographicOrder===state)state.loginInFlight=false;});
    }
    if(state.state==='login') {
      // Public method chooser can be opened, but identity-provider forms and
      // challenges are never guessed or bypassed.
      if(!state.loginOpened && typeof window.fnLogin==='function'){state.loginOpened=true;window.fnLogin();}
      return result('waitingLogin');
    }
    if(state.state==='waitingForm') {
      const birth=document.querySelector('.popup.mapApplication.online input[name="onBrthdy"]');
      if(birth && birth!==state.previousBirth && visible(birth))return result('form');
    }
    return result(state.state,state.message,{diagnostics:input.diagnostics===true && state.state==='blocked'?
      {stage:'openForm',step:state.validationStep||'authentication'}:undefined});
  }
  if (input.command === 'form' || input.command === 'submit') {
    const forms=Array.from(document.querySelectorAll('.popup.mapApplication.online')).filter(visible);
    if(forms.length>1)return result('blocked','신청서가 여러 개 열려 있습니다. 공식 화면에서 현재 도엽 신청서를 확인해 주세요.');
    const form=forms[0];
    if(!visible(form))return result('waitingForm');
    const birth=form.querySelector('input[name="onBrthdy"]'), purpose=form.querySelector('#onPurchsPurps');
    const detail=form.querySelector('input[name="onDetailCn"]');
    const basis=form.querySelector('input[name="onBasis"]');
    if(!birth || !purpose || purpose.tagName!=='SELECT')return result('blocked','신청서 구조가 달라 자동 입력을 중단했습니다. 포털에서 신청서를 확인해 주세요.');
    if(!birth.value.trim() && input.birthDate){birth.value=input.birthDate;changed(birth);}
    if(!purpose.value && input.purpose && Array.from(purpose.options).some(o=>o.value==='BAAAC00008')){
      purpose.value='BAAAC00008';changed(purpose);
    }
    if(purpose.value==='BAAAC00008' && detail && !detail.value.trim() && input.purpose){detail.value=input.purpose;changed(detail);}
    if(basis && !basis.value.trim() && input.purpose){basis.value=input.purpose;changed(basis);}
    if(!birth.value.trim())return result('input','신청에 필요한 생년월일이 비어 있습니다. 아래 입력란 또는 공식 신청서에 입력해 주세요.');
    if(!purpose.value || (purpose.value==='BAAAC00008' && (!detail || !detail.value.trim())))
      return result('input','사용목적이 비어 있습니다. 아래 입력란 또는 공식 신청서에 입력해 주세요.');
    if(isDem && basis && !basis.value.trim())
      return result('input','신청에 필요한 상세용도가 비어 있습니다. 아래 입력란 또는 공식 신청서에 입력해 주세요.');
    const agree=form.querySelector('input[name="onChkAgree"]#agree');
    const submit=button(form,'다운로드');
    if(!agree || !['radio','checkbox'].includes(agree.type) || submit.length!==1 || typeof window.checkValidation!=='function')
      return result('blocked','준수사항 또는 다운로드 버튼을 정확히 확인하지 못했습니다. 공식 신청서에서 계속해 주세요.');
    if(input.command==='form')return result('ready');
    agree.checked=true;changed(agree);
    if(!window.checkValidation(isDem?'4':'1','online'))return result('input','공식 신청서의 필수 항목을 확인해 주세요.');
    const events=window.jQuery && window.jQuery(document);
    if(!events || typeof events.on!=='function' || typeof events.off!=='function')
      return result('blocked','공식 신청 결과를 확인할 수 없어 자동 제출을 중단했습니다.');
    const submission=window.__kaTopographicSubmission={key,number:input.number,state:'pending',started:Date.now()};
    events.off('ajaxComplete.kaTopographicOrder');
    const orderEndpoint = isDem ? '/pd/shbtManage/shbtInsertTritVidoDem.do' : '/pd/shbtManage/shbtInsertNRM.do';
    events.on('ajaxComplete.kaTopographicOrder',function(event,xhr,settings){
      if(window.__kaTopographicSubmission!==submission)return;
      let url;try{url=new URL(settings.url,location.href);}catch(e){return;}
      if(url.origin!==location.origin || url.pathname!==orderEndpoint)return;
      // Observe only outcome flags. Never retain request bodies or personal data.
      let data=xhr.responseJSON;
      if(!data){try{data=JSON.parse(xhr.responseText);}catch(e){submission.state='failed';return;}}
      if(xhr.status<200 || xhr.status>=300 || !data || data.resultCode!=='1' || !data.allDataMap){submission.state='failed';return;}
      submission.state=data.allDataMap.orderDownList ? 'ready' : 'delayed';
    });
    submit[0].click();
    return result('submitted');
  }
  if(input.command==='order') {
    const submission=window.__kaTopographicSubmission;
    if(!submission || submission.key!==key || submission.number!==input.number)
      return result('blocked','신청 결과를 확인하지 못했습니다. 중복 신청하지 않도록 공식 신청 내역을 확인해 주세요.');
    if(submission.state==='ready')return result('orderReady');
    if(submission.state==='delayed')return result('delayed','신청은 접수됐지만 파일 생성이 지연되고 있습니다. 공식 신청 내역에서 준비 상태를 확인해 주세요.');
    if(submission.state==='failed')return result('blocked','공식 서버가 신청 완료를 확인하지 못했습니다. 공식 신청 내역을 확인한 뒤 다시 진행해 주세요.');
    if(Date.now()-submission.started>30000)return result('blocked','공식 신청 응답이 지연되고 있습니다. 중복 신청하지 않고 신청 내역 확인을 기다립니다.');
    return result('pendingOrder');
  }
  if(input.command==='files' || input.command==='download') {
    if(document.readyState!=='complete')return result('waitingFiles');
    if(location.pathname==='/nlippd/pd/innorix/innorixdownLoad.do' && document.getElementById('fileControl')) {
      const control=window.control;
      if(!control || typeof control.getDownloadFiles!=='function' ||
          typeof control.getOption!=='function' || typeof control.download!=='function')return result('waitingFiles');
      const options=control.getOption();
      if(!options || options.agent!==false)return result('blocked','공식 브라우저 전송을 준비하지 못했습니다. 다시 받기를 눌러 주세요.');
      const files=control.getDownloadFiles();
      if(!Array.isArray(files) || !files.length)return result('waitingFiles');
      const records=Array.isArray(input.records)?input.records:[{num:input.number}];
      const numbers=records.map(r=>String(r.num));
      const sheetRe=isDem?/^\d{5,6}$/:/^\d{6}$/;
      if(!numbers.length || numbers.some(n=>!sheetRe.test(n)) || new Set(numbers).size!==numbers.length)
        return result('blocked','신청 도엽 목록을 확인하지 못했습니다.');
      const allowedExt=isDem?/\.(zip|img|tif|tiff|dem|txt|asc|xml)$/i:/\.(zip|dxf|xml)$/i;
      const payloadExt=isDem?/\.(zip|img|tif|tiff|dem|txt|asc)$/i:/\.(dxf|zip)$/i;
      const names=new Set(),urls=new Set(),ids=new Set(),manifest=[];let total=0;
      for(const file of files) {
        const name=file.printFileName,bytes=Number(file.fileSize);
        let url;try{url=new URL(file.downloadUrl,location.href);}catch(e){return result('blocked','공식 파일 주소가 올바르지 않습니다.');}
        let matched=typeof name==='string'?numbers.filter(n=>new RegExp('(^|[^0-9])'+n+'([^0-9]|$)').test(name)):[];
        if(isDem && matched.length!==1 && typeof name==='string') {
          const byName=records.filter(r=>r && r.name && name.includes(String(r.name))).map(r=>String(r.num));
          if(byName.length===1)matched=byName;
          else if(numbers.length===1)matched=[numbers[0]];
        }
        if(matched.length!==1 || typeof file.id!=='string' || !file.id || file.id.length>256 || ids.has(file.id) ||
            !allowedExt.test(name) || names.has(name) || urls.has(url.href) ||
            !Number.isSafeInteger(bytes) || bytes<=0 || url.origin!==location.origin ||
            url.pathname!=='/nlippd/pd/innorix/innorixDownLoad2.do' ||
            url.searchParams.get('fileName')!==name || !url.searchParams.get('directory'))
          return result('blocked','신청 도엽과 공식 파일 목록이 일치하지 않아 전송을 중단했습니다.');
        names.add(name);urls.add(url.href);ids.add(file.id);total+=bytes;
        manifest.push({id:file.id,name,number:matched[0],bytes});
      }
      if(total>1000000000)return result('blocked','선택한 파일 크기가 1GB를 넘습니다.');
      if(numbers.some(n=>!manifest.some(f=>f.number===n && payloadExt.test(f.name))))
        return result('blocked','일부 신청 도엽의 지도 파일이 준비되지 않았습니다.');
      const snapshot=JSON.stringify(files.map(f=>[f.id,f.printFileName,f.fileSize,f.downloadUrl]));
      if(input.command==='files'){
        window.__kaSdkManifest={key,snapshot};
        return result('ready','',{totalBytes:total,files:files.length,serial:true,manifest});
      }
      if(!window.__kaSdkManifest || window.__kaSdkManifest.key!==key || window.__kaSdkManifest.snapshot!==snapshot)
        return result('blocked','공식 파일 목록이 변경되어 전송을 중단했습니다.');
      const index=input.fileIndex===undefined?0:input.fileIndex;
      if(!Number.isInteger(index) || index<0 || index>=files.length)return result('blocked','다음 파일 순서를 확인하지 못했습니다.');
      const token=key+':'+index;
      if(window.__kaSdkDownload===token)return result('downloading');
      window.__kaSdkDownload=token;
      // Use the deployed official SDK's HTML5 downloader. Its anchor creation
      // is only a start: Qt's received bytes/completion drive saving and import.
      if(control.download(files[index].id)===false)return result('blocked','공식 파일 전송을 시작하지 못했습니다.');
      return result('downloading');
    }
    // Final transfer HTML is not a public API. Only clearly identified checked
    // sheet rows and an exact selection-download action can be automated.
    const candidates=[];
    for(const check of document.querySelectorAll('input[type="checkbox"]')){
      const row=check.closest('tr'); if(!row || !visible(row) || check.disabled)continue;
      const text=Array.from(row.querySelectorAll('td,th')).map(cell=>cell.textContent || '').join(' ');
      if(!new RegExp('(^|[^0-9])'+input.number+'([^0-9]|$)').test(text) || !/\.(zip|dxf|xml)\b/i.test(text))continue;
      const size=text.match(/([0-9]+(?:[,.][0-9]+)*)\s*(GB|MB|KB|B)\b/i);
      if(!size)return result('blocked','다운로드할 파일의 크기를 확인하지 못했습니다. 공식 목록에서 내려받아 주세요.');
      const bytes=Number(size[1].replace(/,/g,''))*({B:1,KB:1024,MB:1048576,GB:1073741824}[size[2].toUpperCase()]);
      if(!Number.isFinite(bytes) || bytes<0)return result('blocked','파일 크기 표기를 확인하지 못했습니다.');
      candidates.push({check,bytes});
    }
    const action=button(document,'선택 다운로드');
    if(!candidates.length || action.length!==1){
      const diagnostic={};
      if(input.diagnostics===true){
        diagnostic.scripts=Array.from(document.scripts).filter(s=>s.src).slice(0,64).map(s=>{const u=new URL(s.src,location.href);return u.origin+u.pathname;});
        diagnostic.globals=Object.keys(window).filter(k=>/^[A-Za-z_$][A-Za-z0-9_$]{0,79}$/.test(k) && /innorix|inno|download|control/i.test(k)).slice(0,64);
      }
      return result('blocked','공식 파일 전송 방식을 확인하지 못했습니다. 공식 화면에서 내려받기를 계속할 수 있습니다.',{diagnostics:diagnostic});
    }
    const total=candidates.reduce((sum,c)=>sum+c.bytes,0);
    if(total>1000000000)return result('blocked','이 도엽의 파일 크기가 1GB를 넘습니다. 공식 파일 목록에서 나누어 내려받아 주세요.');
    if(input.command==='files')return result('ready','',{totalBytes:total,files:candidates.length});
    document.querySelectorAll('input[type="checkbox"]').forEach(c=>{c.checked=false;});
    candidates.forEach(c=>{c.check.checked=true;changed(c.check);});
    action[0].click();return result('downloading');
  }
  return result('blocked','지원되지 않는 자동화 단계입니다.');
})
