#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TCanvas.h>
#include <TString.h>
#include <iostream>
#include <vector>
#include <tuple>
#include <string>
#include <sstream>

void input_gain() {
    // Condições originais
    std::vector<std::tuple<double, double, double, std::string>> conditions = {
        {1.0, 1, 70, "Ar"},
        {3.0, 1, 70, "Ar"},
        {3.0, 2, 70, "Ar"},
        {1.0, 1, 90, "Kr"},
        {3.0, 1, 90, "Kr"}
    };

    std::vector<std::string> names;
    for (const auto& [window, pressure, proportion, type] : conditions) {
        std::ostringstream oss;
        oss << type << "_" << proportion << "_" << (100 - proportion)
            << "_" << pressure << "atm_" << window << "cm";
        names.push_back(oss.str());
    }

    // Condições para "multi"
    std::vector<std::tuple<double, double, double, std::string>> multi_conditions = {
        {1.0, 1, 70, "Ar"},
        {1.0, 1, 70, "Ar"},
        {3.0, 2, 70, "Ar"},
        {1.0, 1, 90, "Kr"},
        {1.0, 1, 90, "Kr"}
    };

    std::vector<std::string> multinames;
    for (const auto& [window, pressure, proportion, type] : multi_conditions) {
        std::ostringstream oss2;
        oss2 << type << "_" << proportion << "_" << (100 - proportion)
             << "_" << pressure << "atm_" << window << "cm";
        multinames.push_back(oss2.str());
    }

    // Abrir arquivos
    TFile* file = TFile::Open("no_gain.root");
    TTree* tree = (TTree*) file->Get("no_gain");

    TFile* file2 = TFile::Open("ganho.root");
    TTree* gain_tree = (TTree*) file2->Get("multiplication");

    // Verifica se tamanhos batem
    if (names.size() != multinames.size()) {
        std::cerr << "Erro: tamanhos de 'names' e 'multinames' diferentes!\n";
        return;
    }

    // Loop com índice para percorrer ambos simultaneamente
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string& nome = names[i];
        const std::string& mult_nome = multinames[i];

        std::cout << "Processando: " << nome << " e " << mult_nome << std::endl;

        int nelectrons = 0;
        tree->SetBranchAddress(nome.c_str(), &nelectrons);

        int ganho = 0;
        gain_tree->SetBranchAddress(mult_nome.c_str(), &ganho);

        TH1F* pdf = new TH1F(("pdf_" + nome).c_str(), "Gain PDF", 1000, 0, 600);
        Long64_t nentries_gain = gain_tree->GetEntries();
        for (Long64_t j = 0; j < nentries_gain; ++j) {
            gain_tree->GetEntry(j);
            pdf->Fill(ganho);
        }
        pdf->Scale(1.0 / pdf->Integral(), "width");

        TH1F* spectra = new TH1F(("spectra_" + nome).c_str(), nome.c_str(), 512, 0, 2 * 4096);

        Long64_t nentries = tree->GetEntries();
        for (Long64_t j = 0; j < nentries; ++j) {
            tree->GetEntry(j);

            int sum = 0;
            for (int k = 0; k < nelectrons; ++k) {
                sum += pdf->GetRandom();  // simula o ganho de cada elétron
            }
            spectra->Fill(sum);
        }

        TCanvas* c = new TCanvas(("c_" + nome).c_str(), "Canvas", 800, 600);
        spectra->GetYaxis()->SetTitle("Counts");
        spectra->GetXaxis()->SetTitle("Spectrum [# electrons]");
        //spectra->GetXaxis()->CenterTitle();
        //spectra->GetYaxis()->CenterTitle();
        spectra->Draw();

        c->SaveAs(("../results/"+ nome + ".png").c_str());

        delete pdf;
        delete spectra;
        delete c;
    }

    file->Close();
    file2->Close();
}
