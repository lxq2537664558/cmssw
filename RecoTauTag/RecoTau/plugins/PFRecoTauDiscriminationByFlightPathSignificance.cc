#include "RecoTauTag/RecoTau/interface/TauDiscriminationProducerBase.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include <FWCore/ParameterSet/interface/ConfigurationDescriptions.h>
#include <FWCore/ParameterSet/interface/ParameterSetDescription.h>

/* class PFRecoTauDiscriminationByFlightPathSignificance
 * created : August 30 2010,
 * contributors : Sami Lehti (sami.lehti@cern.ch ; HIP, Helsinki)
 * based on H+ tau ID by Lauri Wendland
 */

#include "TrackingTools/TransientTrack/interface/TransientTrack.h"
#include "TrackingTools/TransientTrack/interface/TransientTrackBuilder.h"
#include "TrackingTools/Records/interface/TransientTrackRecord.h"
#include "RecoVertex/KalmanVertexFit/interface/KalmanVertexFitter.h"
#include "RecoVertex/VertexPrimitives/interface/TransientVertex.h"
#include "RecoBTag/SecondaryVertex/interface/SecondaryVertex.h"
#include "RecoTauTag/RecoTau/interface/RecoTauVertexAssociator.h"

#include "TLorentzVector.h"

using namespace reco;
using namespace std;
using namespace edm;

class PFRecoTauDiscriminationByFlightPathSignificance
  : public PFTauDiscriminationProducerBase  {
  public:
    explicit PFRecoTauDiscriminationByFlightPathSignificance(const ParameterSet& iConfig)
      :PFTauDiscriminationProducerBase(iConfig){
      flightPathSig		= iConfig.getParameter<double>("flightPathSig");
      withPVError		= iConfig.getParameter<bool>("UsePVerror");
      booleanOutput 		= iConfig.getParameter<bool>("BooleanOutput");
      //      edm::ConsumesCollector iC(consumesCollector());
      vertexAssociator_ = new reco::tau::RecoTauVertexAssociator(iConfig.getParameter<ParameterSet>("qualityCuts"),consumesCollector());
    }

    ~PFRecoTauDiscriminationByFlightPathSignificance() override{}

    void beginEvent(const edm::Event&, const edm::EventSetup&) override;
    double discriminate(const reco::PFTauRef&) const override;

    static void fillDescriptions(edm::ConfigurationDescriptions & descriptions);
  private:
    double threeProngFlightPathSig(const PFTauRef&) const ;
    double vertexSignificance(reco::Vertex const&,reco::Vertex const &,GlobalVector const&) const;

    reco::tau::RecoTauVertexAssociator* vertexAssociator_;

    bool booleanOutput;
    double flightPathSig;
    bool withPVError;

    const TransientTrackBuilder* transientTrackBuilder;
};

void PFRecoTauDiscriminationByFlightPathSignificance::beginEvent(
    const Event& iEvent, const EventSetup& iSetup){

   vertexAssociator_->setEvent(iEvent);

  // Transient Tracks
  edm::ESHandle<TransientTrackBuilder> builder;
  iSetup.get<TransientTrackRecord>().get("TransientTrackBuilder",builder);
  transientTrackBuilder = builder.product();

}

double PFRecoTauDiscriminationByFlightPathSignificance::discriminate(const PFTauRef& tau) const{

  if(booleanOutput) return ( threeProngFlightPathSig(tau) > flightPathSig ? 1. : 0. );
  return threeProngFlightPathSig(tau);
}

double PFRecoTauDiscriminationByFlightPathSignificance::threeProngFlightPathSig(
    const PFTauRef& tau) const {
  double flightPathSignificance = 0;

  reco::VertexRef primaryVertex = vertexAssociator_->associatedVertex(*tau);

  if (primaryVertex.isNull()) {
    edm::LogError("FlightPathSignficance") << "Could not get vertex associated"
      << " to tau, returning -999!" << std::endl;
    return -999;
  }

  //Secondary vertex
  vector<TransientTrack> transientTracks;
  for(const auto& pfSignalCand : tau->signalPFChargedHadrCands()){
    if(pfSignalCand->trackRef().isNonnull()){
      const TransientTrack transientTrack = transientTrackBuilder->build(pfSignalCand->trackRef());
      transientTracks.push_back(transientTrack);
    }
    else if(pfSignalCand->gsfTrackRef().isNonnull()){
      const TransientTrack transientTrack = transientTrackBuilder->build(pfSignalCand->gsfTrackRef());
      transientTracks.push_back(transientTrack);
    }
  }
  if(transientTracks.size() > 1){
    KalmanVertexFitter kvf(true);
    TransientVertex tv = kvf.vertex(transientTracks);

    if(tv.isValid()){
      GlobalVector tauDir(tau->px(),
          tau->py(),
          tau->pz());
      Vertex secVer = tv;
      // We have to un-const the PV for some reason
      reco::Vertex primaryVertexNonConst = *primaryVertex;
      flightPathSignificance = vertexSignificance(primaryVertexNonConst,secVer,tauDir);
    }
  }
  return flightPathSignificance;
}

double PFRecoTauDiscriminationByFlightPathSignificance::vertexSignificance(
    reco::Vertex const & pv, Vertex const & sv,GlobalVector const & direction) const {
  return SecondaryVertex::computeDist3d(pv,sv,direction,withPVError).significance();
}

void
PFRecoTauDiscriminationByFlightPathSignificance::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  // pfRecoTauDiscriminationByFlightPathSignificance
  edm::ParameterSetDescription desc;
  desc.add<double>("flightPathSig", 1.5);
  desc.add<edm::InputTag>("PVProducer", edm::InputTag("offlinePrimaryVertices"));
  desc.add<bool>("BooleanOutput", true);
  desc.add<edm::InputTag>("PFTauProducer", edm::InputTag("pfRecoTauProducer"));
  {
    edm::ParameterSetDescription psd0;
    psd0.add<std::string>("BooleanOperator", "and");
    {
      edm::ParameterSetDescription psd1;
      psd1.add<double>("cut");
      psd1.add<edm::InputTag>("Producer");
      psd0.addOptional<edm::ParameterSetDescription>("leadTrack", psd1);
    }
    desc.add<edm::ParameterSetDescription>("Prediscriminants", psd0);
  }
  desc.add<bool>("UsePVerror", true);
  descriptions.add("pfRecoTauDiscriminationByFlightPathSignificance", desc);
}

DEFINE_FWK_MODULE(PFRecoTauDiscriminationByFlightPathSignificance);
