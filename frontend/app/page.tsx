import Station from '@/components/station'
import { Handheld } from '@/components/pixels'

export default function Page() {
  return <><header><div className="nav"><div className="brand"><span className="brandmark"><Handheld /></span>Pocket Arcade</div><span className="online"><i/> System online</span></div></header><main><div className="intro"><p className="eyebrow"><span/> EMULATOR STATION</p><h1>GameBoy Emulator</h1><p>Insert a cartridge. Revisit the classics.</p></div><Station/><footer><span>POCKET ARCADE</span><span>8-BIT EMULATION SYSTEM</span></footer></main></>
}
