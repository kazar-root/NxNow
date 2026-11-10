import { AppletType, HidNpadButton } from '@nx.js/constants';
import { installNsp } from '@nx.js/install-title';
import { NcmStorageId } from '@nx.js/ncm';
import { NACP } from '@tootallnate/nacp';
import { useEffect, useState } from 'react';
import { Rect, Text } from 'react-tela';
import { render } from 'react-tela/render';
import type { Module } from './hacbrewpack';
import { loadForwarderKeys } from './prod-keys';
import { deterministicTitleId } from './util';

// Installation progress is rendered in the app; dependency debug messages
// contain internal content IDs and paths that do not belong in this UI.
console.debug = () => {};

const requestPath = 'sdmc:/switch/NXNow/forwarder_request.json';
const resultPath = 'sdmc:/switch/NXNow/forwarder_result.json';
let exitEnabled = false;
let buttonsReleased = false;
let completedAt = 0;
let returnPath = '';

function waitForPaint(): Promise<void> {
  return new Promise((resolve) => requestAnimationFrame(() => resolve()));
}

function closeInstaller(): never {
  try {
    if (returnPath.startsWith('sdmc:/') && /\.nro$/i.test(returnPath)) {
      new Switch.Application(returnPath).launch();
    }
  } catch {
    // Fall back to the loader if the source NRO was moved or removed.
  }
  Switch.exit();
}

function keepInstallerAlive() {
  const rawButtons = navigator.getGamepads()[0]?.rawButtons ?? 0n;
  const pressed = (rawButtons & BigInt(HidNpadButton.A | HidNpadButton.B)) !== 0n;
  if (!exitEnabled) buttonsReleased = false;
  else {
    if (!pressed) buttonsReleased = true;
    if ((buttonsReleased && pressed) || (completedAt > 0 && Date.now() - completedAt >= 5000))
      closeInstaller();
  }
  requestAnimationFrame(keepInstallerAlive);
}
// Keep Plus from triggering nx.js's default immediate exit while writing a title.
addEventListener('beforeunload', (event) => event.preventDefault());

// Keep a native animation handle active from first paint through completion.
// Without this, nx.js may stop after the install promise settles before React's
// final state update reaches the framebuffer.
requestAnimationFrame(keepInstallerAlive);

interface InstallRequest {
  version: 1;
  name: string;
  author: string;
  displayVersion: string;
  nroPath: string;
  args: string;
  language?: string;
  returnPath?: string;
}

function readRequest(): InstallRequest {
  const data = Switch.readFileSync(requestPath);
  if (!data) throw new Error('NxNow forwarder request is missing.');
  const request = JSON.parse(new TextDecoder().decode(data)) as InstallRequest;
  if (
    request.version !== 1 ||
    !request.name ||
    !request.nroPath.startsWith('sdmc:/') ||
    !request.args.includes(request.nroPath) ||
    request.name.length > 511 ||
    request.args.length > 1800
  ) {
    throw new Error('NxNow forwarder request is invalid.');
  }
  return request;
}

async function generateNsp(request: InstallRequest): Promise<Uint8Array> {
  const wasm = Switch.readFileSync('romfs:/hacbrewpack.wasm');
  const main = Switch.readFileSync('romfs:/template/exefs/main');
  const mainNpdm = Switch.readFileSync('romfs:/template/exefs/main.npdm');
  if (!wasm || !main || !mainNpdm) {
    throw new Error('Forwarder generator resources are incomplete.');
  }

  const app = new Switch.Application(request.nroPath);
  if (!app.icon) throw new Error('The shortcut NRO has no embedded icon.');
  const icon = app.icon;
  const titleId = await deterministicTitleId(request.nroPath, request.args);
  const keys = loadForwarderKeys();
  const createModule = (await import('./hacbrewpack.js')).default;

  return await new Promise<Uint8Array>((resolve, reject) => {
    let module: Module;
    void createModule({
      noInitialRun: true,
      print() {},
      printErr() {},
      wasmBinary: wasm,
      preRun(instance: Module) {
        module = instance;
        const { FS } = instance;
        FS.writeFile('/keys.dat', keys);
        FS.mkdir('/exefs');
        FS.writeFile('/exefs/main', new Uint8Array(main));
        FS.writeFile('/exefs/main.npdm', new Uint8Array(mainNpdm));

        const nacp = new NACP();
        nacp.id = titleId;
        nacp.title = request.name;
        nacp.author = request.author || 'KazaR-RooT';
        nacp.version = request.displayVersion || '0.1';
        nacp.startupUserAccount = 0;
        nacp.userAccountSaveDataSize = 0n;
        nacp.userAccountSaveDataJournalSize = 0n;

        FS.mkdir('/control');
        FS.writeFile('/control/control.nacp', new Uint8Array(nacp.buffer));
        FS.writeFile('/control/icon_AmericanEnglish.dat', new Uint8Array(icon));
        FS.mkdir('/romfs');
        FS.writeFile('/romfs/nextArgv', new TextEncoder().encode(request.args));
        FS.writeFile(
          '/romfs/nextNroPath',
          new TextEncoder().encode(request.nroPath),
        );
      },
      onRuntimeInitialized() {
        try {
          const exitCode = module.callMain([
            '--nopatchnacplogo',
            '--titleid',
            titleId,
            '--nologo',
            '--plaintext',
          ]);
          if (exitCode !== 0) {
            reject(new Error(`Forwarder generator exited with code ${exitCode}.`));
            return;
          }
          const name = module.FS
            .readdir('/hacbrewpack_nsp')
            .find((entry) => entry.endsWith('.nsp'));
          if (!name) throw new Error('Generated NSP could not be found.');
          resolve(module.FS.readFile(`/hacbrewpack_nsp/${name}`));
        } catch (error) {
          reject(error);
        }
      },
    }).catch(reject);
  });
}

const messages: Record<string, string[]> = {
  en: ['Creating shortcut', 'Please wait…', 'Shortcut created', 'A or B to close · 5 seconds', 'Could not create shortcut', 'Check your SD card and installation keys.', 'A or B to close'],
  fr: ['Création du raccourci', 'Veuillez patienter…', 'Raccourci créé', 'A ou B pour fermer · 5 secondes', 'Impossible de créer le raccourci', 'Vérifiez la carte SD et les clés d’installation.', 'A ou B pour fermer'],
  es: ['Creando acceso directo', 'Espera…', 'Acceso directo creado', 'A o B para cerrar · 5 segundos', 'No se pudo crear el acceso directo', 'Comprueba la tarjeta SD y las claves de instalación.', 'A o B para cerrar'],
  it: ['Creazione collegamento', 'Attendere…', 'Collegamento creato', 'A o B per chiudere · 5 secondi', 'Impossibile creare il collegamento', 'Controlla la scheda SD e le chiavi di installazione.', 'A o B per chiudere'],
  pl: ['Tworzenie skrótu', 'Proszę czekać…', 'Skrót utworzony', 'A lub B, aby zamknąć · 5 sekund', 'Nie udało się utworzyć skrótu', 'Sprawdź kartę SD i klucze instalacji.', 'A lub B, aby zamknąć'],
  ru: ['Создание ярлыка', 'Подождите…', 'Ярлык создан', 'A или B — закрыть · 5 секунд', 'Не удалось создать ярлык', 'Проверьте SD-карту и ключи установки.', 'A или B — закрыть'],
  uk: ['Створення ярлика', 'Зачекайте…', 'Ярлик створено', 'A або B — закрити · 5 секунд', 'Не вдалося створити ярлик', 'Перевірте SD-картку та ключі встановлення.', 'A або B — закрити'],
  'zh-CN': ['正在创建快捷方式', '请稍候…', '快捷方式已创建', '按 A 或 B 关闭 · 5 秒', '无法创建快捷方式', '请检查 SD 卡和安装密钥。', '按 A 或 B 关闭'],
};

function Installer() {
  const [heading, setHeading] = useState('NxNow');
  const [detail, setDetail] = useState('…');
  const [failed, setFailed] = useState(false);

  useEffect(() => {
    function writeResult(result: object) {
      try {
        Switch.writeFileSync(resultPath, JSON.stringify(result));
      } catch {
        // The on-screen result remains available if SD result logging fails.
      }
    }

    let text = messages.en;
    async function install() {
      const request = readRequest();
      text = messages[request.language ?? "en"] ?? messages.en;
      returnPath = request.returnPath ?? "";
      setHeading(text[0]);
      setDetail(text[1]);
      if (Switch.appletType() !== AppletType.Application) {
        throw new Error(
          'Full-memory/application mode is required to install a forwarder.',
        );
      }

      const nsp = await generateNsp(request);


      for await (const _event of installNsp(
        new Blob([nsp]),
        NcmStorageId.SdCard,
      )) {
        // The installer validates and commits each content record in order.
      }

      writeResult({
        version: 1,
        success: true,
        name: request.name,
        installedAt: Date.now(),
      });
      Switch.removeSync(requestPath);
      setHeading(text[2]);
      setDetail(text[3]);
      await waitForPaint();
      await waitForPaint();
      completedAt = Date.now();
      exitEnabled = true;
    }

    void install().catch(async (error: unknown) => {
      const message = error instanceof Error ? error.message : String(error);
      writeResult({ version: 1, success: false, error: message });
      setFailed(true);
      setHeading(text[4]);
      setDetail(`${text[5]}\n${text[6]}`);
      await waitForPaint();
      await waitForPaint();
      exitEnabled = true;
    });
  }, []);

  return (
    <>
      <Rect fill='#0a0816' height={720} width={1280} x={0} y={0} />
      <Rect fill='#b17bff' height={284} width={1084} x={98} y={218} />
      <Rect fill='#171126' height={280} width={1080} x={100} y={220} />
      <Text fill={failed ? '#ff9b9b' : '#f7f3ff'} fontSize={38} textAlign='center' x={640} y={296}>
        {heading}
      </Text>
      {detail.split('\n').map((line, index) => (
        <Text key={`${index}-${line}`} fill='#d4c8e8' fontSize={23} textAlign='center' x={640} y={370 + index * 36}>
          {line}
        </Text>
      ))}
    </>
  );
}

render(<Installer />, screen);
